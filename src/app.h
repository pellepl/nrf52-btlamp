/*
 * app.h
 *
 *  Created on: Mar 1, 2017
 *      Author: petera
 */

#ifndef APP_H_
#define APP_H_

#include "system_config.h"
#include "ble_flash.h"

#define TNV_BUF_SIZE              BLE_FLASH_PAGE_SIZE
#define TNV_ID_BITS               4
#define TNV_LEN_BITS              5

void app_init(void);
void app_on_connected(void);
void app_on_disconnected(void);
void app_on_data(uint8_t *data, uint16_t len);

void start_softdevice(void); // in main.c, yeah, pretty ugly
#endif /* APP_H_ */
