# Radish

A small Redis clone built from scratch in C, to learn how Redis actually works internally — the event loop, the wire protocol, client state, command dispatch — by building it rather than just reading about it.

## Why

Following [Arpit Bhayani's Redis-from-scratch playlist](https://www.youtube.com/playlist?list=PLsdq-3Z1EPT0eElcdOON9fdaeaQjlyXDt), but in C instead of Go. Redis itself is written in C — building in C means using the same primitives Redis uses (raw POSIX sockets, manual `poll`/`epoll`, manual memory management) instead of having a runtime hide the event loop from you. The goal is  tot learn everything in depth: single-threaded event loop, non-blocking I/O, and the RESP protocol, implemented by hand.

## Current stage

- Non-blocking TCP server working: `socket()`/`bind()`/`listen()`/`accept()`, `poll()`-based event loop, per-client read/write buffering (`src/server.c`).
- Per-client connection state (`client_t`) implemented: input/output buffers, parser state fields, `argv`/`argc` (`src/client.h`, `src/client.c`).
- RESP parsing started (`src/resp.h`, `src/resp.c`): `parse_crlf_terminated_integer()` parses length-prefix fields (`*N\r\n`, `$L\r\n`) out of a possibly-partial buffer, returning `PARSE_OK` / `PARSE_NEED_MORE` / `PARSE_ERR`.
- Full client parsing state machine designed (`PARSE_START → PARSE_ARRAY_LEN ↔ PARSE_BULK_LEN ↔ PARSE_BULK_DATA → dispatch`, with pipelining and error-close transitions) but not yet wired into a driver function.

## What's next

1. `parse_inbuf()` — the driver function that walks the state machine per client on each `poll()` iteration.
2. Raw fixed-length read for `PARSE_BULK_DATA` (read exactly `current_len` bytes, then trailing `\r\n` — not a CRLF scan, since bulk string content can itself contain `\r\n`).
3. Command dispatch: `argv[0]` → handler.
4. In-memory key-value store (hash table).
5. `PING`, `SET`, `GET` commands.
6. Migrate `poll()` → `epoll()` once the protocol layer is solid.

## Building

```
clang-format -i <file>   # format a file before committing
```

(Makefile and `clang-tidy` linting planned, not yet added.)

## Project layout

```
server.c       — main loop, poll(), accept, connection lifecycle
client.h/.c    — client_t struct, add_client, close_client, queue_bytes
resp.h/.c      — RESP parser (in progress), response encoder (not started)
commands.h/.c  — SET, GET, PING handlers (not started)
```
