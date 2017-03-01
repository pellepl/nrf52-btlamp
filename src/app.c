
#include "app.h"
#include "nrf_gpio.h"
#include "miniutils.h"
#include "nrf_drv_spi.h"
#include "app_timer.h"
#define BITMANIO_STORAGE_BITS 8
#define BITMANIO_H_WHEREABOUTS "bitmanio.h"
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


typedef struct anim_s {
  uint32_t rgb;
  uint16_t ms;
} anim_t;

APP_TIMER_DEF(tim_id);
const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);
static struct app {
  uint8_t spi_ws_buf[RGB_DATA_LEN];
  int anim;
  int anim_ix;
  uint32_t rgb[WS2812B_LEDS];
  uint32_t lamp_rgb;
  volatile bool lamp_dirty;
  volatile bool lamp_tx;
}app;

static void ws2812b_make_buffer(uint32_t *rgb) {
  bitmanio_array8_t bio_arr;
  bitmanio_init_array8(&bio_arr, app.spi_ws_buf, CODED_BYTES_PER_RGB_BYTE);
  uint32_t bix = 0;
  int i, j;
  for (i = 0; i < WS2812B_LEDS; i++) {
    uint32_t d = *rgb++;
    d =
        ((d & 0xff0000) >> 8) |
        ((d & 0x00ff00) << 8) |
        ((d & 0x0000ff));
    for (j = 8*3-1; j >= 0; j--) {
      bitmanio_set8(&bio_arr, bix++, (d >> j) & 1 ? CODE1 : CODE0);
    }
  }
}

static void spi_handler(nrf_drv_spi_evt_t const * p_event) {
  print("spi.finished dirty:%i\n", app.lamp_dirty);
  if (app.lamp_dirty) {
    app.lamp_dirty = false;
    app_timer_start(tim_id, APP_TIMER_TICKS(25, APP_TIMER_PRESCALER), NULL);
  }
  app.lamp_tx = false;
}

static void app_lamp_tx(void) {
  app.lamp_tx = true;
  ws2812b_make_buffer(app.rgb);
  nrf_drv_spi_transfer(&spi, app.spi_ws_buf, sizeof(app.spi_ws_buf), NULL, 0);
}

static void app_lamp_update(void) {
  print("app.lamp_update tx:%i\n", app.lamp_tx);
  if (app.lamp_tx) {
    app.lamp_dirty = true;
  } else {
    app.lamp_dirty = false;
    app_lamp_tx();
  }
}

static void app_lamp_set(uint32_t rgb) {
  app.lamp_rgb = rgb;
  int i;
  for (i = 0; i < WS2812B_LEDS; i++) app.rgb[i] = rgb;
  app_lamp_update();
}


static void start_anim(int anim) {
  app.anim = anim;
  app.anim_ix = 0;
  if (anim == 0) {
    app_timer_stop(tim_id);
    app_lamp_set(app.lamp_rgb);
    return;
  }

  app_timer_start(tim_id, APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), NULL);
}

static void anim_timer(void * p_context) {
  uint32_t call_again = 0;
  app.anim_ix++;
  switch (app.anim) {
  case ANIM_CONNECT:
    memset(app.rgb, 0, sizeof(app.rgb));
    app.rgb[app.anim_ix-1] = 0x00ff00;
    app_lamp_update();
    call_again = app.anim_ix >= WS2812B_LEDS ? 0 : 100;
    break;
  case ANIM_DISCONNECT:
    memset(app.rgb, 0, sizeof(app.rgb));
    app.rgb[WS2812B_LEDS - app.anim_ix] = 0xff0000;
    app_lamp_update();
    call_again = app.anim_ix >= WS2812B_LEDS ? 0 : 100;
    break;
  }
  if (call_again == 0) {
    start_anim(ANIM_NONE);
  } else {
    app_timer_start(tim_id, APP_TIMER_TICKS(call_again, APP_TIMER_PRESCALER), NULL);
  }
}

void app_on_connected(void) {
  start_anim(ANIM_CONNECT);
}

void app_on_disconnected(void) {
  start_anim(ANIM_DISCONNECT);
}

void app_init(void) {
  uint32_t err_code;
  memset(&app, 0, sizeof(app));
  app.lamp_rgb = 0x442208;
  print("app: starting test timer\n");

  {
    err_code = app_timer_create(&tim_id, APP_TIMER_MODE_SINGLE_SHOT, anim_timer);
    print("app: tim_creat res %i\n", err_code);
  }

  print("app: starting test spi\n");

  {

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

    app_lamp_set(app.lamp_rgb);

  }
}
