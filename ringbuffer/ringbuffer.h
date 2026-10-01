#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct ringbuffer_instance_t* ringbuffer_t;

ringbuffer_t ringbuffer_create(uint8_t capacity);
uint8_t ringbuffer_capacity(ringbuffer_t instance);
bool ringbuffer_read(ringbuffer_t instance, uint8_t item);
bool ringbuffer_write(ringbuffer_t instance, uint8_t item);
bool ringbuffer_destroy(ringbuffer_t instance);

#endif