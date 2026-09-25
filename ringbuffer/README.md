# Ring Buffers

A design guide for software engineering students.

This is not a solution write-up. It walks you through _why_ a ring buffer exists, _what_ state it must track, and _how_ to split that design across C header and source files. You will write the C yourself.

Companion slides: _Designing the Ultimate Waiting Room for Asynchronous Data_ (`Ring_Buffer_Engineering.pdf`). Read the slides and this guide together. The slides are the mechanical picture. This file is the design discussion and the lab brief.

---

## How to use this guide

1. Read a section.
2. Draw the picture it describes on paper before you type.
3. Only then translate the idea into your own C.
4. Do not paste finished implementations out of a tutorial. If a block here looks like code, it is **pseudocode**: it names steps, not the exact C.

Suggested file names (use them consistently):

```text
ringbuffer/
  ringbuffer.h
  ringbuffer.c
  test_ringbuffer.c
  Makefile
  README.md
```

Prefix public functions with `rb_` so the API has a small, obvious namespace: `rb_push`, `rb_pop`, and so on.

---

## What you should be able to explain when you are done

- Why serial hardware forces a producer/consumer split.
- Why a shifting array is the wrong waiting room.
- What `head`, `tail`, `count`, and `capacity` each mean.
- Why a slot can still hold old bytes after those bytes have been read.
- How wrap-around creates the illusion of a loop in a finite array.
- Why “buffer full” is a signal, not a mysterious crash.
- What belongs in a `.h` file versus a `.c` file.
- How to prove the design with a 4-slot wrap sequence.

If you can walk a classmate through those points at a whiteboard, the C will follow.

---

## 1. The problem: hardware does not wait

Bytes arrive one at a time over USB-serial (UART RX). The other side of the wire does not ask whether your main loop is free. If you are busy blinking an LED, running a control step, or writing a log line, the next byte still shows up **now**.

Without a waiting room, that byte is lost.

The waiting room is the ring buffer. It sits between “byte arrived” and “I am ready to look at it.”

Draw this pipeline and label the three stages:

```text
HOST TERMINAL                 WIRE                    MCU
  "l e d ENTER"  ----UART RX---->  [ waiting room ]  ---->  main loop
```

---

## 2. Split the work: producer vs consumer

Two roles share the same memory. They must not do each other’s job.

| Role     | Typical actor                         | Verb                                   | Pointer it owns |
| -------- | ------------------------------------- | -------------------------------------- | --------------- |
| Producer | Receive interrupt, or a tight poll    | **Push** a byte in immediately         | Head (write)    |
| Consumer | Main loop, when it has a spare moment | **Pop** a byte out and assemble a line | Tail (read)     |

The producer is fast and unforgiving. The consumer is allowed to be late — up to the size of the waiting room. When the room is full, the producer has nowhere to put the next byte. That is back-pressure.

Think about this before you write a single function: _who is allowed to move head, and who is allowed to move tail?_ If both move both, you do not have a queue.

---

## 3. Why not a normal array?

A beginner’s first idea:

```text
METHOD: shifting array
  read slot 0
  copy slot 1 -> slot 0
  copy slot 2 -> slot 1
  copy slot 3 -> slot 2
  ...
```

Every pop copies every remaining byte. Cost grows with how much data is waiting. On a microcontroller that is wasted CPU in the exact place you cannot afford it.

A ring buffer does something cheaper:

```text
METHOD: pointer
  read the slot named by tail
  move tail to the next slot
  do not move the other bytes
```

Old data never slides. You only update two integers. You also never reallocate. Capacity is fixed at setup time. That is the whole performance argument.

---

## 4. Anatomy

Treat a fixed row of slots as a loop.

You need four pieces of state:

```text
RingBuffer
    slots[CAPACITY]   // the physical memory
    head              // next write index
    tail              // next read index
    count             // how many unread bytes are in the room
```

Meanings, in plain language:

- **Capacity** — how many seats the room has. Does not change after init.
- **Head** — the seat where the next arrival sits.
- **Tail** — the seat the next departing guest leaves from.
- **Count** — occupied seats. Empty means `count = 0`. Full means `count = capacity`.

`head == tail` is _not_ enough to know empty vs full by itself. After a wrap, those two indexes can meet for either reason. That is why this design keeps an explicit `count`. (There is another school that wastes one slot instead of storing `count`. Use `count` for this lab so every seat is usable.)

Important physical fact from the slides: **we never erase**. After a pop, the old byte may still sit in memory. It does not matter. Tail has moved past it, so that seat is free to be overwritten on a later push.

---

## 5. Walk the machine through five states

Use capacity 4. Keep a paper grid. After each event, write the four slots, then `head`, `tail`, and `count`.

### State 0 — empty

Both pointers start at slot 0. Count is 0. A pop here must fail. A peek here must fail.

```text
[ _ ][ _ ][ _ ][ _ ]
  ^
 head and tail
 count = 0
```

### State 1 — asynchronous burst

The host sends `l`, `e`, `d`. The producer pushes all three before the consumer runs. Head advances. Tail stays put. Count becomes 3.

```text
[ l ][ e ][ d ][ _ ]
  ^              ^
 tail           head
 count = 3
```

Question to answer on paper: after three pushes starting at index 0, what is `head`?

### State 2 — the consumer catches up

Main loop pops once and receives `l`. Tail moves to the next occupied seat. Slot 0 still physically holds `l`. That is stale data, not live data.

```text
[ l ][ e ][ d ][ _ ]
       ^         ^
      tail      head
 count = 2
```

Question: why would writing `0` into slot 0 after the pop be wasted work?

### State 3 — the wrap

Two more bytes arrive: a space, then `o`. The space takes the last unused seat. The next write has no slot 4, so head returns to slot 0 and overwrites the stale `l`.

That wrap is the entire “ring.” The array is linear. The index math makes it circular.

```text
[ o ][ e ][ d ][   ]
       ^    ^
     tail  head     (draw this yourself; confirm both indexes)
 count = 4
```

Question: do you wrap the index _before_ or _after_ you store the byte? Get this order wrong and the wrap test will fail in a predictable way later.

### State 4 — full

Count equals capacity. A fifth push must fail in version 1. Do not overwrite the oldest byte yet. Fail-on-full is easier to test.

A full buffer is not a mystery bug. It is the waiting room saying the consumer lagged the producer.

---

## 6. Design decisions you must make before coding

Write a one-line answer for each. These choices belong in comments at the top of `ringbuffer.h`.

1. **Who owns the slot array?**  
   Preferred for embedded work: the _caller_ provides storage (`uint8_t storage[N]`). The library does not call `malloc` on the hot path.

2. **What happens on push-when-full?**  
   Version 1: reject the push, leave the buffer unchanged, report failure.  
   Stretch later: drop the oldest byte (UART RX style).

3. **What happens on pop-when-empty?**  
   Report failure. Do not invent a byte. Do not write through a caller pointer if you have nothing to give.

4. **How do you tell full from empty?**  
   Use `count`. Do not try to encode both states with `head == tail` alone.

5. **What is peek?**  
   Look at the next byte the consumer would pop, but do not move tail and do not change count.

6. **Does reset free the slot array?**  
   No. Reset only clears head, tail, and count. The caller still owns the memory.

If two students make different choices here, their APIs will silently disagree. Decide first.

---

## 7. Structuring `.h` and `.c`

C programs are split so that _users of a module see a contract_ and _the compiler of that module sees the machinery_.

### What a header is

`ringbuffer.h` is the public contract. Another file should be able to `#include "ringbuffer.h"` and compile against your API without reading `ringbuffer.c`.

Put in the header:

- Include guards, so a file can include the header twice without exploding.
- The includes _your contract itself_ needs (`stddef.h`, `stdint.h`, `stdbool.h` if those types appear in prototypes).
- The data type the rest of the program is allowed to name.
- Function prototypes.
- Short comments that state success vs failure for each call.

Do **not** put in the header:

- The bodies of `rb_push` / `rb_pop`.
- `printf` debugging.
- Test helpers.
- Static file-local helper functions.

Shape, not the answer:

```text
// ringbuffer.h  — public contract only

guard the file so it is safe to include more than once

bring in only the types the prototypes need

describe RingBuffer as a group of:
    pointer to caller-owned slots
    capacity
    head
    tail
    count

declare:
    init(buffer, storage, capacity)
    reset(buffer)
    push(buffer, byte)        -> success or failure
    pop(buffer, out_byte)     -> success or failure
    peek(buffer, out_byte)    -> success or failure
    count(buffer)
    free_space(buffer)
    is_empty(buffer)
    is_full(buffer)
```

Why the struct fields are listed in the header for this lab: students need to _see_ the state while they learn. In production firmware you may later hide the fields (opaque pointer) so callers cannot poke `head` directly. Do not hide them yet. Do, however, treat those fields as private by convention: tests talk to the API, not to `buffer.head`.

### What a source file is

`ringbuffer.c` is the machinery. It includes its own header and implements every prototype.

Put in the source:

- `#include "ringbuffer.h"` first, so the compiler checks that the header is self-contained.
- The function bodies.
- File-local helpers, marked `static`, if you need them (for example a tiny `advance_index` used by both push and pop).
- No `main`. A library file is not a program.

Shape:

```text
// ringbuffer.c  — machinery only

include "ringbuffer.h"

init:
    remember the storage pointer and the capacity
    set head, tail, and count to the empty state

reset:
    return head, tail, and count to empty
    do not forget the storage pointer
    do not change capacity

push / pop / peek:
    follow the producer and consumer pseudocode below

queries:
    empty, full, count, and free seats are all questions about count vs capacity
```

### What the test file is

`test_ringbuffer.c` is a _client_ of the library. It includes the header, creates a `RingBuffer` and a small storage array, and asks questions through the API. It has `main`. It does not reach into `ringbuffer.c` internals.

That three-file split is the C habit this lab is teaching, as much as the data structure itself:

| File                | Role           | Has `main`? |
| ------------------- | -------------- | ----------- |
| `ringbuffer.h`      | Contract       | No          |
| `ringbuffer.c`      | Implementation | No          |
| `test_ringbuffer.c` | Client / proof | Yes         |

If you put `main` in `ringbuffer.c`, you cannot later link that same `.c` into a firmware image. Keep the library reusable.

### Include-guard sketch (still not a solution)

```text
if this header has not been seen yet
    mark it seen
    ...declarations...
end
```

The usual C spelling is a `#ifndef` / `#define` / `#endif` pair named after the file, such as `RINGBUFFER_H`. Pick one name and use it.

### Who includes what

```text
test_ringbuffer.c          ringbuffer.c
        \                     /
         \                   /
          -> ringbuffer.h <-
```

`test_ringbuffer.c` should **not** include `ringbuffer.c`. The Makefile compiles both `.c` files and links them.

Makefile responsibilities, in words:

- Compile with a strict C11 baseline and warnings treated as errors.
- Build one test program from `ringbuffer.c` and `test_ringbuffer.c`.
- Provide `make test` that runs the program and `make clean` that deletes it.

Write the Makefile yourself. Match the file names you actually chose.

---

## 8. Operations, in pseudocode only

Translate these into C. Do not expect a compiler to accept this text.

### Initialize

```text
function Init(buffer, storage, capacity):
    remember storage as the slot array
    remember capacity
    head <- 0
    tail <- 0
    count <- 0
```

Decide what you do if `storage` is missing or `capacity` is 0. Either document “caller must not do that” or return an error. Do not mix the two policies.

### Push — producer

```text
function Push(buffer, byte):
    if buffer is full:
        fail safely          // leave state unchanged
        return failure

    write byte into slots[head]
    move head forward by 1
    if head is past the last slot:
        wrap head back to 0    // you choose the wrap expression
    count <- count + 1
    return success
```

The wrap is the design puzzle. Common tools are “if past the end, set to 0” or a remainder against capacity. Either is acceptable if it matches your tests. Do not copy a line of C from a website until you can draw why it works.

### Pop — consumer

```text
function Pop(buffer, out):
    if buffer is empty:
        do not write out
        return failure

    read slots[tail] into out
    move tail forward by 1
    if tail is past the last slot:
        wrap tail back to 0
    count <- count - 1
    return success
```

### Peek

Same read as pop. No movement. No change to count. Failure when empty.

### Queries

```text
empty      when count is 0
full       when count equals capacity
free seats when capacity minus count
```

---

## 9. Prove it: the 4-slot wrap test

This sequence is the exam. Capacity is 4. If your pointers do not match these states, the math is wrong.

| Action          | Physical memory                           | Pointer math                         |
| --------------- | ----------------------------------------- | ------------------------------------ |
| Push 1, 2, 3, 4 | `[1] [2] [3] [4]`                         | tail = 0, head = 0, count = 4 (full) |
| Pop twice       | `[1] [2] [3] [4]` (1 and 2 are now stale) | tail = 2, head = 0, count = 2        |
| Push 5, 6       | `[5] [6] [3] [4]`                         | tail = 2, head = 2, count = 4 (full) |

Then pop the rest. The live bytes, in order, must be **3, 4, 5, 6**.

### If the leftover pops are wrong

| What you popped | Likely mistake                                                       |
| --------------- | -------------------------------------------------------------------- |
| `3, 4, 1, 2`    | Head and tail are being updated on the wrong side of the store/load. |
| `5, 6, 3, 4`    | You wrapped the index _before_ storing the byte.                     |
| `3, 4, 5, 6`    | Wrap is doing what it should.                                        |

Also test these cases. Each one exists to catch a different off-by-one:

1. Fresh buffer: pop and peek fail.
2. One byte in, peek sees it, pop returns it, buffer empty again.
3. Fill to capacity, then push fails and count stays at capacity.
4. Drain a full buffer, then pop fails.
5. The wrap sequence above.
6. Peek twice: same value, count unchanged.
7. Fill, reset, empty, then you can fill again.
8. Capacity 1: push, full, pop, empty.

A test that only uses capacity 256 will hide wrap bugs. Use 4 and 1 on purpose.

Write tests as separate functions that return success or failure. Print the file and line when a check fails. `main` should run them in order and stop on the first failure. Tests talk to the API, not to `head` and `tail` fields.

---

## 10. Lab rules for version 1

1. **Fail on full.** Overwrite-oldest is a stretch goal. Rejecting a full push is much easier to test.
2. **Never erase old slots.** Move indexes. Leave stale bytes alone.
3. **Keep a clean namespace.** Files: `ringbuffer.h` / `ringbuffer.c`. Functions: `rb_…`.
4. **No heap on the hot path.** Caller owns storage. `malloc` does not belong inside push or pop.
5. **No printing inside the library.** Logging belongs in tests or in a later logger module.
6. **Strict compile.** Warnings are defects for this lab.

---

## 11. Suggested build order

Do not implement everything at once.

1. Header contract and empty source stubs that compile.
2. Makefile and a test file that fails for the right reasons.
3. `init` / `reset` and the query functions.
4. `push` and `pop` against the empty, fill, and drain cases.
5. Wrap case on paper, then in the test.
6. `peek`.
7. Capacity 1.

Only after those pass should you consider stretch work: block read/write of N bytes, overwrite-on-full, power-of-two capacity with a mask instead of a wrap branch, or a small C++ wrapper that holds the C struct and a reference to storage (no heap, no exceptions).

---

## 12. Check your understanding

Answer in complete sentences, without looking at your `.c` file.

1. A UART interrupt and a main loop both need the same bytes. Who pushes, who pops, and why must those roles stay separate?
2. After a pop, slot `tail_old` still holds the byte you just returned. Is the buffer wrong?
3. Why is `head == tail` ambiguous if you do not also store `count`?
4. Why does the wrap test push four values into a four-slot buffer, pop two, then push two more, instead of only filling and draining?
5. What would break later if `main` lived inside `ringbuffer.c`?
6. Name three things that belong in `ringbuffer.h` and three that do not.

When those answers are easy, implement the library.
