# Ring Buffer

Byte ring buffer as a small reusable C library. No UI. No STL. No `malloc` on the hot path.

A ring buffer is a fixed-size array used as a queue. Writes go at the head, reads come from the tail, and both indices wrap. Full push fails. Empty pop fails.

You will reuse this shape for UART RX, log bursts, and stepper command queues on the Nucleo.

## Goal

Ship four files that compile with `make test` and exit 0:

```
ringbuf/
  ringbuf.h
  ringbuf.c
  test_ringbuf.c
  Makefile
  README.md
```

Work C-first. Compile with:

```text
-std=c11 -Wall -Wextra -Wpedantic -Werror -g
```

## Contract

- Capacity is fixed at init.
- Caller owns the backing storage (`uint8_t storage[N]`).
- Push and pop are O(1).
- Wrap-around must work.
- Full and empty must be distinguishable.
- Library functions do not call `printf`.
- Use a `count` field so all `N` slots are usable.

## Suggested work order

Do these in order. Do not jump to stretch items.

- [ ] Create the files listed above
- [ ] Write `ringbuf.h` exactly as specified below
- [ ] Write an empty `ringbuf.c` that compiles (stubs that return false / 0)
- [ ] Write the Makefile
- [ ] Write `test_ringbuf.c` with the `EXPECT` helper and all required tests
- [ ] Confirm `make test` builds and the tests fail for the right reasons
- [ ] Implement `rb_init` and `rb_reset`
- [ ] Implement `rb_count`, `rb_free`, `rb_is_empty`, `rb_is_full`
- [ ] Implement `rb_push` and `rb_pop`
- [ ] Implement `rb_peek`
- [ ] Get every required test passing
- [ ] Draw a 4-slot buffer on paper and walk the wrap test by hand
- [ ] Fill in the short README section at the bottom if you publish this repo

## API

```c
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *data;
    size_t   cap;
    size_t   head;
    size_t   tail;
    size_t   count;
} ringbuf_t;

void   rb_init(ringbuf_t *rb, uint8_t *storage, size_t cap);
void   rb_reset(ringbuf_t *rb);

bool   rb_push(ringbuf_t *rb, uint8_t byte);   /* false if full */
bool   rb_pop(ringbuf_t *rb, uint8_t *out);    /* false if empty */
bool   rb_peek(ringbuf_t *rb, uint8_t *out);   /* false if empty; does not consume */

size_t rb_count(const ringbuf_t *rb);
size_t rb_free(const ringbuf_t *rb);
bool   rb_is_empty(const ringbuf_t *rb);
bool   rb_is_full(const ringbuf_t *rb);

#endif
```

Keep the API this small.

### Behavior

| Call | Success | Failure |
| --- | --- | --- |
| `rb_push` | stores byte, advances head, increments count, returns true | buffer full: unchanged, returns false |
| `rb_pop` | writes oldest byte to `*out`, advances tail, decrements count, returns true | empty: does not write `*out`, returns false |
| `rb_peek` | writes oldest byte to `*out`, does not change head/tail/count, returns true | empty: does not write `*out`, returns false |
| `rb_init` | stores pointer and cap, zeros head/tail/count | caller must not pass `NULL` storage or `cap == 0` |
| `rb_reset` | zeros head/tail/count; leaves `data` and `cap` alone | — |

Index update after a successful push:

```c
rb->data[rb->head] = byte;
rb->head = (rb->head + 1) % rb->cap;
rb->count++;
```

Index update after a successful pop:

```c
*out = rb->data[rb->tail];
rb->tail = (rb->tail + 1) % rb->cap;
rb->count--;
```

## Makefile

```make
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -g

.PHONY: test clean

test: test_ringbuf
	./test_ringbuf

test_ringbuf: ringbuf.c test_ringbuf.c ringbuf.h
	$(CC) $(CFLAGS) -o $@ ringbuf.c test_ringbuf.c

clean:
	rm -f test_ringbuf
```

`make test` is the only done signal.

## Tests

`test_ringbuf.c` is the app. It constructs buffers, runs checks, prints pass/fail, and returns non-zero on failure.

Use this helper:

```c
#define EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)
```

You need `<stdio.h>`, `<stdint.h>`, `<stdbool.h>`, and `"ringbuf.h"`.

Write each case as its own function that returns `0` on success and `1` on failure. `main` should run all of them and return the first failure.

### Required cases

1. **Empty.** Fresh buffer: `pop` and `peek` return false.
2. **Single byte.** Push `0xA5`. Peek sees it. Pop returns it. Buffer is then empty.
3. **Fill to capacity.** `N` successful pushes, then push returns false. `rb_count == N`.
4. **Drain.** After a full buffer, `N` successful pops, then pop returns false.
5. **Wrap.** Capacity 4. Push `1,2,3,4`. Pop twice. Push `5,6`. Pops must yield `3,4,5,6` in that order.
6. **Peek does not consume.** Peek twice. Same value both times. Count unchanged.
7. **Reset.** Fill, reset, empty, count 0, can fill again.
8. **Capacity 1.** Push, full, pop, empty. Tiny buffers catch off-by-one bugs.

Do not treat `printf` inspection as a test. Every claim goes through `EXPECT`.

Do not test only with capacity 256. Cases 5 and 8 exist to catch wrap bugs.

### Suggested `main` shape

```c
int main(void)
{
    if (test_empty())      return 1;
    if (test_single())     return 1;
    if (test_fill())       return 1;
    if (test_drain())      return 1;
    if (test_wrap())       return 1;
    if (test_peek())       return 1;
    if (test_reset())      return 1;
    if (test_cap_one())    return 1;
    printf("PASS\n");
    return 0;
}
```

## Implementation notes

- `rb_init` must set `data`, `cap`, and zero `head`, `tail`, and `count`.
- Push when full: return false and leave the buffer unchanged.
- Pop when empty: return false and do not write `*out`.
- `% cap` is fine for this version.
- Do not call `malloc` inside the library.
- Do not hide wrap bugs by oversizing the test buffer.

### Optional init policy

The spec treats `storage == NULL` or `cap == 0` as a caller error. Either document that and assume valid input, or add an `int rb_init(...)` later that returns an error. Do not mix the two in v1.

## Definition of done

- [ ] `make test` exits 0
- [ ] All eight required cases exist and can fail independently
- [ ] You can explain wrap-around out loud with a 4-slot drawing
- [ ] README states: count model (full capacity, no wasted slot), API, and `make test`

## Stretch (after tests pass)

Do not start these until `make test` is green.

- [ ] `size_t rb_write(ringbuf_t *rb, const uint8_t *buf, size_t n)`
- [ ] `size_t rb_read(ringbuf_t *rb, uint8_t *buf, size_t n)`
- [ ] Overwrite mode: push-on-full drops the oldest byte (UART RX style)
- [ ] Power-of-two capacity and `index & (cap - 1)` instead of `%`
- [ ] C++ wrapper: class that holds a `ringbuf_t` plus a reference to storage, no heap, no exceptions

Next drill after this: length-prefixed packet codec on top of the ring buffer.
