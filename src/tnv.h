/*
 * tnv.h
 *
 *  Created on: Mar 2, 2017
 *      Author: petera
 */


#ifndef TNV_H_
#define TNV_H_

#include "system.h"
#define BITMANIO_STORAGE_BITS     8
#define BITMANIO_H_WHEREABOUTS "bitmanio.h"
#define BITMANIO_HEADER
#include BITMANIO_H_WHEREABOUTS

#ifndef TNV_BUF_SIZE
#define TNV_BUF_SIZE              512
#endif
#ifndef TNV_ID_BITS
#define TNV_ID_BITS               4
#endif
#ifndef TNV_LEN_BITS
#define TNV_LEN_BITS              5
#endif

typedef uint32_t (* tnv_buf_write_fn_t)(uint8_t *buf, uint32_t offs, uint32_t len, uint8_t *src);
typedef uint32_t (* tnv_buf_erase_fn_t)(uint8_t *buf);

typedef struct tnv_s {
  bitmanio_stream8_t str;
  uint8_t *buf;
  struct {
    uint32_t value  : 32;
    uint32_t bits   : TNV_LEN_BITS;
    uint8_t defined : 1;
    uint8_t dirty :   1;
  } __attribute__(( packed )) cache[1<<TNV_ID_BITS];
  tnv_buf_write_fn_t write;
  tnv_buf_erase_fn_t erase;
} tnv_t;


void tnv_init(tnv_t *tnv,
              uint8_t *buf,
              tnv_buf_write_fn_t write,
              tnv_buf_erase_fn_t erase);

void tnv_set(tnv_t *tnv, uint8_t id, uint8_t bits, uint32_t value);

uint32_t tnv_get(tnv_t *tnv, uint8_t id, uint32_t def);

uint32_t tnv_commit(tnv_t *tnv);


#endif /* TNV_H_ */
