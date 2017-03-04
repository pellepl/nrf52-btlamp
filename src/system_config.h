/*
 * system_config.h
 *
 *  Created on: Mar 1, 2017
 *      Author: petera
 */

#ifndef SYSTEM_CONFIG_H_
#define SYSTEM_CONFIG_H_

#include "system.h"

// NOTE: have uart pins to 0xffffffff (disconnected) when
//       running non-debug. Otherwise things hang when booting.
#define PIN_UART_RX_NUMBER    0xffffffff//6
#define PIN_UART_TX_NUMBER    0xffffffff//8

#define PIN_MOSI_NUMBER       0

#define DEVICE_NAME                     "Pelles BT lampa"                               /**< Name of device. Will be included in the advertising data. */

#define APP_TIMER_PRESCALER             0                                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE         8                                           /**< Size of timer operation queues. */

#endif /* SYSTEM_CONFIG_H_ */
