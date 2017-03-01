/*
 * miniutils_config.h
 *
 *  Created on: Sep 7, 2013
 *      Author: petera
 */

#ifndef MINIUTILS_CONFIG_H_
#define MINIUTILS_CONFIG_H_

// enable formatted float printing support
#define MINIUTILS_PRINT_FLOAT
// enable formatted longlong printing support
#define MINIUTILS_PRINT_LONGLONG
// enabel base64 encoding/decoding
//#define MINIUTILS_BASE64

#include "app_uart.h"

#define PUTC(p, c)  \
    app_uart_put((c));
#define PUTB(p, b, l)  \
    do { \
      u8_t *___buf = (u8_t *)(b); \
      int ___l = (int)(l); \
      int ___i; \
      for (___i = 0; ___i < ___l; ___i++) app_uart_put(*___buf++); \
    } while (0);


#endif /* MINIUTILS_CONFIG_H_ */
