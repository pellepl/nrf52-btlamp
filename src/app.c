
#include "app.h"
#include "nrf_gpio.h"
#include "miniutils.h"
#include "nrf_drv_spi.h"
#include "app_timer.h"
#include "hardfault.h"
#include "softdevice_handler.h"
#include "ble_flash.h"
#include "tnv.h"

#define BITMANIO_STORAGE_BITS 8
#define BITMANIO_H_WHEREABOUTS "bitmanio.h"
#define BITMANIO_HEADER
#include BITMANIO_H_WHEREABOUTS

#define CODED_BYTES_PER_RGB_BYTE  5
#define WS2812B_LEDS              16
#define RGB_DATA_LEN \
  (3 * WS2812B_LEDS * CODED_BYTES_PER_RGB_BYTE)

#define CODE0 0b10000
#define CODE1 0b11110

#define ANIM_NONE         0
#define ANIM_CONNECT      1
#define ANIM_DISCONNECT   2
#define ANIM_WRITE        3
#define ANIM_ERROR        4

#define TNV_RGB           1
#define TNV_INTENSITY     2
#define TNV_USER_VAL      15

static void settings_read(void);

typedef struct anim_s {
  uint32_t rgb;
  uint16_t ms;
} anim_t;

static const uint8_t GAMMA[] = {
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

APP_TIMER_DEF(tim_anim_id);
APP_TIMER_DEF(tim_lamp_id);
APP_TIMER_DEF(tim_ctrl_id);
const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);
static struct app {
  uint8_t spi_ws_buf[RGB_DATA_LEN];
  int anim;
  int anim_ix;
  uint32_t rgb[WS2812B_LEDS];
  uint32_t lamp_rgb;
  uint32_t lamp_intens;
  volatile bool lamp_dirty;
  volatile bool lamp_tx;
  volatile bool factory_reset;
  volatile bool startup;
  tnv_t tnv;
} app;

static void ws2812b_make_buffer(uint32_t *rgb) {
  bitmanio_array8_t bio_arr;
  bitmanio_init_array8(&bio_arr, app.spi_ws_buf, CODED_BYTES_PER_RGB_BYTE);
  uint32_t bix = 0;
  int i, j;
  for (i = 0; i < WS2812B_LEDS; i++) {
    uint32_t d = *rgb++;
    uint16_t r = (d>>16)&0xff;
    uint16_t g = (d>>8)&0xff;
    uint16_t b = (d)&0xff;
    if (app.lamp_intens < 10) {
      uint8_t intens = GAMMA[(sizeof(GAMMA) * app.lamp_intens) / 10];
      r = (r * intens) >> 8;
      g = (g * intens) >> 8;
      b = (b * intens) >> 8;
    }
    r &= 0xff;
    g &= 0xff;
    b &= 0xff;

    d = (g << 16) | (r << 8) | b;
    for (j = 8*3-1; j >= 0; j--) {
      bitmanio_set8(&bio_arr, bix++, (d >> j) & 1 ? CODE1 : CODE0);
    }
  }
}

static void spi_handler(nrf_drv_spi_evt_t const * p_event) {
  //print("spi.finished dirty:%i\n", app.lamp_dirty);
  app.lamp_tx = false;
  if (app.lamp_dirty) {
    app.lamp_dirty = false;
    app_timer_start(tim_lamp_id, APP_TIMER_TICKS(5, APP_TIMER_PRESCALER), NULL);
  }
}

static void lamp_tx(void) {
  app.lamp_tx = true;
  ws2812b_make_buffer(app.rgb);
  nrf_drv_spi_transfer(&spi, app.spi_ws_buf, sizeof(app.spi_ws_buf), NULL, 0);
}

static void lamp_update(void) {
  //print("app.lamp_update tx:%i\n", app.lamp_tx);
  if (app.lamp_tx) {
    app.lamp_dirty = true;
  } else {
    app.lamp_dirty = false;
    lamp_tx();
  }
}

static void lamp_set_color(uint32_t rgb, bool store) {
  app.lamp_rgb = rgb;
  print("app.lamp_color:%06x\n", rgb);
  int i;
  for (i = 0; i < WS2812B_LEDS; i++) {
    app.rgb[i] = rgb ? rgb : (rand_next() & 0xffffff);
  }
  lamp_update();
  if (store) tnv_set(&app.tnv, TNV_RGB, rgb);
}

static void lamp_set_intensity(uint32_t i, bool store) {
  app.lamp_intens = i;
  print("app.lamp_intensity:%i\n", i);
  lamp_update();
  if (store) tnv_set(&app.tnv, TNV_INTENSITY, i);
}

static void lamp_store_user_value(uint32_t x) {
  tnv_set(&app.tnv, TNV_USER_VAL, x);
}


static void start_anim(int anim) {
  app.anim = anim;
  app.anim_ix = 0;
  if (anim == 0) {
    app_timer_stop(tim_anim_id);
    lamp_set_color(app.lamp_rgb, FALSE);
    return;
  }

  app_timer_start(tim_anim_id, APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), NULL);
}

static void anim_timer(void * p_context) {
  uint32_t call_again = 0;
  app.anim_ix++;
  switch (app.anim) {
  case ANIM_CONNECT: {
    int i;
    memset(app.rgb, 0, sizeof(app.rgb));
    for (i = 1; i < 8; i++) {
      app.rgb[(app.anim_ix -1 + i) % WS2812B_LEDS] = 0x002211 * i;
    }
    lamp_update();
    call_again = app.anim_ix >= WS2812B_LEDS*5 ? 0 : 40;
    break;
  }
  case ANIM_DISCONNECT: {
    int i;
    memset(app.rgb, 0, sizeof(app.rgb));
    for (i = 1; i < 8; i++) {
      app.rgb[(5*WS2812B_LEDS - app.anim_ix - i) % WS2812B_LEDS] = 0x220011 * i;
    }
    lamp_update();
    call_again = app.anim_ix >= WS2812B_LEDS*5 ? 0 : 40;
    break;
  }
  case ANIM_WRITE: {
    memset(app.rgb, 0, sizeof(app.rgb));
    app.rgb[(5*WS2812B_LEDS - app.anim_ix) % WS2812B_LEDS] = 0xff00ff;
    lamp_update();
    call_again = app.anim_ix >= WS2812B_LEDS*1 ? 0 : 40;
    break;
  }
  case ANIM_ERROR: {
    int i;
    memset(app.rgb, 0, sizeof(app.rgb));
    if (app.anim_ix & 1) {
      for (i = 0; i < WS2812B_LEDS/2; i++) {
        if ((app.anim_ix>>1) & 1) {
          app.rgb[(WS2812B_LEDS/2 + i) % WS2812B_LEDS] = 0xff0000;
        } else {
          app.rgb[i] = 0xff0000;
        }
      }
    }
    lamp_update();
    call_again = app.anim_ix >= 20 ? 0 : 400;
    break;
  }
  }
  if (call_again == 0) {
    start_anim(ANIM_NONE);
  } else {
    app_timer_start(tim_anim_id, APP_TIMER_TICKS(call_again, APP_TIMER_PRESCALER), NULL);
  }
}
static void lamp_timer(void * p_context) {
  lamp_update();
}

uint32_t flash_write_fn(uint8_t *buf, uint32_t offs, uint32_t len, uint8_t *src) {
  start_anim(ANIM_WRITE);
  // align src to 32 bits
  uint32_t start = (uint32_t)(buf + offs);
  uint32_t end = (uint32_t)(buf + offs + len);
  uint32_t start_align_pre = (start & 3);
  uint32_t end_align_post = (end & 3) == 0 ? 0 : (4 - (end & 3));
  uint32_t aligned_size = (end + end_align_post) - (start - start_align_pre);
  uint32_t wbuf[aligned_size/4];
  memset(wbuf, 0xff, aligned_size);
  memcpy((uint8_t *)wbuf + start_align_pre, src, len);
  uint32_t *flash_addr = (uint32_t *)(start - start_align_pre);
  print("app.flash write @ %08x, len %i\n", flash_addr, aligned_size);
  uint32_t err_code;
  err_code = softdevice_handler_sd_disable();

  err_code = ble_flash_block_write(flash_addr, wbuf, aligned_size/4);

  print("app.softdevice enabling\n");

  start_softdevice();
  return err_code;
}

uint32_t flash_erase_fn(uint8_t *buf) {
  lamp_set_color(0x880088, FALSE);
  uint32_t err_code;
  err_code = softdevice_handler_sd_disable();
  print("app.softdevice disabled res %i\n", err_code);
  volatile int a = 0x2000;
  while(a--);

  uint8_t page = (uint32_t)buf / BLE_FLASH_PAGE_SIZE;
  err_code = ble_flash_page_erase(page);
  print("app.flash erase page %i res %i\n", page, err_code);

  print("app.softdevice enabling\n");
  a = 0x2000;
  while(a--);
  start_softdevice();
  lamp_set_color(app.lamp_rgb, FALSE);
  return err_code;
}


static void control_timer(void * p_context) {
  if (app.startup) {
    app.startup = FALSE;
    lamp_set_color(app.lamp_rgb, FALSE);
  } else if (app.factory_reset) {
    app.factory_reset = FALSE;
    flash_erase_fn(app.tnv.buf);
    settings_read();
    lamp_set_color(app.lamp_rgb, FALSE);
  } else {
    tnv_commit(&app.tnv);
  }
}

static void save_trigger(void) {
  app_timer_stop(tim_ctrl_id);
  app_timer_start(tim_ctrl_id, APP_TIMER_TICKS(TIME_COMMIT_MS, APP_TIMER_PRESCALER), NULL);
}

void app_on_connected(void) {
  print("app.on_connected\n");
  start_anim(ANIM_CONNECT);
}

void app_on_disconnected(void) {
  print("app.on_disconnected\n");
  start_anim(ANIM_DISCONNECT);
}

void app_on_data(uint8_t *data, uint16_t len) {
  if (len < 2) return;
  bool trigger_save = TRUE;
  int i;
  for (i = 0; i < len; i++) {
    print("%c", data[i]);
  }
  print("\n");
  if (data[0] == 'i') {
    int intens = atoin((char *)&data[1], 10, len-1);
    intens = MIN(10, intens);
    intens = MAX(0, intens);
    lamp_set_intensity(intens, TRUE);
  }
  else if (data[0] == 's') {
    uint32_t x = atoin((char *)&data[1], 16, len-1);
    lamp_store_user_value(x);
  }
  else if (data[0] == 'c') {
    uint32_t rgb = atoin((char *)&data[1], 16, len-1);
    lamp_set_color(rgb, TRUE);
  }
  else if (len == 4 && strncmp((char *)data, "warm", 4) == 0) {
    lamp_set_color(COLOR_DEFAULT, TRUE);
  }
  else if (len == 4 && strncmp((char *)data, "cold", 4) == 0) {
    lamp_set_color(0xdddddd, TRUE);
  }
  else if (len == 3 && strncmp((char *)data, "red", 3) == 0) {
    lamp_set_color(0xff0000, TRUE);
  }
  else if (len == 5 && strncmp((char *)data, "green", 5) == 0) {
    lamp_set_color(0x00ff00, TRUE);
  }
  else if (len == 4 && strncmp((char *)data, "blue", 4) == 0) {
    lamp_set_color(0x0000ff, TRUE);
  }
  else if (len == 4 && strncmp((char *)data, "cyan", 4) == 0) {
    lamp_set_color(0x00ffff, TRUE);
  }
  else if (len == 6 && strncmp((char *)data, "yellow", 6) == 0) {
    lamp_set_color(0xffff00, TRUE);
  }
  else if (len == 6 && strncmp((char *)data, "orange", 6) == 0) {
    lamp_set_color(0xff8800, TRUE);
  }
  else if (len == 7 && strncmp((char *)data, "magenta", 7) == 0) {
    lamp_set_color(0xff00ff, TRUE);
  }
  else if (len == 5 && strncmp((char *)data, "white", 5) == 0) {
    lamp_set_color(0xdddddd, TRUE);
  }
  else if (len == 6 && strncmp((char *)data, "random", 6) == 0) {
    lamp_set_color(0, TRUE);
  }
  else if (len == 5 && strncmp((char *)data, "error", 5) == 0) {
    start_anim(ANIM_ERROR);
    trigger_save = false;
  }
  else if (len == 6 && strncmp((char *)data, "update", 6) == 0) {
    tnv_reload(&app.tnv);
    trigger_save = false;
  }
  else if (len == 5 && strncmp((char *)data, "reset", 5) == 0) {
    app.factory_reset = TRUE;
  }
  else {
    trigger_save = false;
  }
  if (trigger_save) {
    save_trigger();
  }
}

static void settings_read(void) {
  tnv_init(&app.tnv,
      (uint8_t *)((BLE_FLASH_PAGE_END - 1) * BLE_FLASH_PAGE_SIZE),
      flash_write_fn, flash_erase_fn);
  app.lamp_intens = tnv_get(&app.tnv, TNV_INTENSITY, 5);
  app.lamp_rgb = tnv_get(&app.tnv, TNV_RGB, COLOR_DEFAULT);
  uint32_t user_val = tnv_get(&app.tnv, TNV_USER_VAL, 0);
  print("tnv.int:%i\n", app.lamp_intens);
  print("tnv.rgb:%08x\n", app.lamp_rgb);
  print("tnv.usr:%08x\n", user_val);
}

void app_init(void) {
  uint32_t err_code;
  print("\n\napp.init\n");
  memset(&app, 0, sizeof(app));

  err_code = app_timer_create(&tim_anim_id, APP_TIMER_MODE_SINGLE_SHOT, anim_timer);
  print("app: tim_anim creat res %i\n", err_code);
  err_code = app_timer_create(&tim_lamp_id, APP_TIMER_MODE_SINGLE_SHOT, lamp_timer);
  print("app: tim_lamp creat res %i\n", err_code);
  err_code = app_timer_create(&tim_ctrl_id, APP_TIMER_MODE_SINGLE_SHOT, control_timer);
  print("app: tim_save creat res %i\n", err_code);
  nrf_drv_spi_config_t config = {                                                            \
      .sck_pin      = NRF_DRV_SPI_PIN_NOT_USED,
      .mosi_pin     = PIN_MOSI_NUMBER, //NRF_DRV_SPI_PIN_NOT_USED,
      .miso_pin     = NRF_DRV_SPI_PIN_NOT_USED,
      .ss_pin       = NRF_DRV_SPI_PIN_NOT_USED,
      .irq_priority = SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
      .orc          = 0xFF,
      .frequency    = NRF_DRV_SPI_FREQ_4M,
      .mode         = NRF_DRV_SPI_MODE_3,
      .bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,
  };
  err_code = nrf_drv_spi_init(&spi, &config, spi_handler);
  print("app: spi_init res %i\n", err_code);

  rand_seed(0x12312312);
  settings_read();
  app.startup = TRUE;
  app_timer_start(tim_ctrl_id, APP_TIMER_TICKS(TIME_START_LAMP_MS, APP_TIMER_PRESCALER), NULL);

  print("app.init finished\n");
}

void HardFault_process(HardFault_stack_t * p_stack)
{
  while(1);
}
