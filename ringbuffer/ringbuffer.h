#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>

typedef enum
{
  SLOTS,
  HEAD,
  TAIL,
  COUNT
} RingBuffer;

void ringbuffer_init();
void rb_push(uint8_t head);
void rb_pop(uint8_t tail);

#endif