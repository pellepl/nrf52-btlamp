#include "tnv.h"
#include "miniutils.h"

#define _stream     bitmanio_stream8_t
#define _strwrite   bitmanio_write_z8
#define _strread    bitmanio_read_z8

static void _tnv_write(_stream *bs, uint32_t id, uint32_t val, uint8_t len) {
  uint8_t buf[4];
  int i;
  for (i = 0; i < 4; i++) {
    buf[i] = (val >> (i*8)) & 0xff;
  }
  uint8_t *d = buf;
  _strwrite(bs, id, TNV_ID_BITS);
  _strwrite(bs, len - 1, TNV_LEN_BITS);
  while (len > 0) {
    uint8_t ch = len > 8 ? 8 : len;
    _strwrite(bs, *d++, ch);
    len -= ch;
  }
}

static void _tnv_read(tnv_t *tnv,_stream *str) {
  const uint8_t end_id = 0;
  uint8_t id;
  uint32_t last_pos = bitmanio_getpos8(str);
  do {
    if (last_pos + TNV_ID_BITS >= TNV_BUF_SIZE*8) break;
    id = _strread(str, TNV_ID_BITS);
    if (id == end_id) {
      break;
    }
    if (last_pos + TNV_LEN_BITS >= TNV_BUF_SIZE*8) break;
    uint8_t len = _strread(str, TNV_LEN_BITS) + 1;
    if (last_pos + len >= TNV_BUF_SIZE*8) break;
    tnv->cache[id].bits = len -1;
    uint32_t value = 0;
    while (len > 0) {
      uint8_t ch = len > 8 ? 8 : len;
      uint8_t d = _strread(str, ch);
      value = (value << ch) | d;
      len -= ch;
    }
    tnv->cache[id].value = value;
    tnv->cache[id].defined = 1;
    tnv->cache[id].dirty = 0;
    last_pos = bitmanio_getpos8(str);
  } while (id != end_id);
  bitmanio_setpos8(str, last_pos);
}

void tnv_init(tnv_t *tnv,
              uint8_t *buf,
              tnv_buf_write_fn_t write,
              tnv_buf_erase_fn_t erase) {
  tnv->write = write;
  tnv->erase = erase;
  tnv->buf = buf;
  memset(tnv->cache, 0, sizeof(tnv->cache));
  bitmanio_init_stream8(&tnv->str, tnv->buf);
  _tnv_read(tnv, &tnv->str);
}

void tnv_set(tnv_t *tnv, uint8_t id, uint8_t bits, uint32_t value) {
  tnv->cache[id].bits = bits - 1;
  tnv->cache[id].value = value;
  tnv->cache[id].defined = 1;
  tnv->cache[id].dirty = 1;
}

uint32_t tnv_get(tnv_t *tnv, uint8_t id, uint32_t def) {
  if (tnv->cache[id].defined) {
    return tnv->cache[id].value;
  }
  tnv_set(tnv, id, 32, def);
  return def;
}

uint32_t tnv_commit(tnv_t *tnv) {
  // calculate needed bits for this commit
  int i;
  uint32_t bits_needed = 0;
  for (i = 1; i < 1<<TNV_ID_BITS; i++) {
    if (tnv->cache[i].defined && tnv->cache[i].dirty) {
      bits_needed += TNV_ID_BITS + TNV_LEN_BITS + tnv->cache[i].bits + 1;
      print("tnv:%i dirty, %i bits needed\n", i, bits_needed);
    }
  }
  if (bits_needed == 0) {
    return 0;
  }

  // calculate bits left in page
  uint32_t bits_left = TNV_BUF_SIZE*8 - bitmanio_getpos8(&tnv->str);
  if (bits_needed > bits_left) {
    print("tnv:%i bits needed, have %i - erase\n", bits_needed, bits_left);
    // need more bits than we have, dirtify all defined values and recalc needed bits..
    bits_needed = 0;
    for (i = 1; i < 1<<TNV_ID_BITS; i++) {
      if (tnv->cache[i].defined) {
        tnv->cache[i].dirty = 1;
        bits_needed += TNV_ID_BITS + TNV_LEN_BITS + tnv->cache[i].bits + 1;
      }
    }
    // .. erase page ..
    tnv->erase(tnv->buf);
    // .. and reset stream
    bitmanio_setpos8(&tnv->str, 0);
  }

  // make a ram buffer to write dirty values to
  uint32_t bitpos = bitmanio_getpos8(&tnv->str);
  uint32_t wbuf_len = ((bitpos & 7) + bits_needed + 7) / 8;
  uint8_t wrbuf[wbuf_len];
  memset(wrbuf, 0xff, wbuf_len);
  // initiate first byte with real content
  wrbuf[0] = tnv->buf[bitpos / 8];
  // make stream and position it properly
  _stream wrstr;
  bitmanio_init_stream8(&wrstr, wrbuf);
  bitmanio_setpos8(&wrstr, bitpos & 7);

  // dump all dirty stuff to buffer
  for (i = 1; i < 1<<TNV_ID_BITS; i++) {
    if (tnv->cache[i].defined && tnv->cache[i].dirty) {
      _tnv_write(&wrstr, i, tnv->cache[i].value, tnv->cache[i].bits+1);
      tnv->cache[i].dirty = 0;
      print("tnv:dump %i, bitpos %i\n", i, bitmanio_getpos8(&wrstr));
    }
  }
  // update read stream pointer to what we're about to write
  bitmanio_setpos8(&tnv->str, bitpos + bits_needed);
  // and write
  return tnv->write(tnv->buf, bitpos/8, (bitmanio_getpos8(&wrstr) + 7)/8, wrbuf);
}
