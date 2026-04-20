/**
 * Linear / bump allocator. The core principle is that hot paths must not
 * call malloc; HLE, GPU and audio state lives in arenas that are created
 * up-front and freed wholesale at tear-down.
 *
 * Phase 0 ships the minimum viable arena: alloc + reset + destroy. No
 * freelists, no sub-arenas; those land when a subsystem actually needs them.
 */
#ifndef SWITCH_COMMON_ARENA_H
#define SWITCH_COMMON_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct Arena
{
  uint8_t *base;
  size_t capacity_bytes;
  size_t used_bytes;
  int owns_memory;
} Arena;

/* Create an arena backed by a freshly malloc'd block. */
int arena_create(Arena *out, size_t capacity_bytes);

/* Create an arena over caller-provided memory (e.g. SharedArrayBuffer). */
void arena_create_in_place(Arena *out, void *memory, size_t capacity_bytes);

/* Allocate `size` bytes aligned to `alignment` (power of two). Returns NULL
 * when the arena is full. Never mallocs. */
void *arena_allocate(Arena *arena, size_t size, size_t alignment);

/* Rewind to empty. Existing pointers become invalid. */
void arena_reset(Arena *arena);

/* Destroy. If the arena owns its memory, frees it. */
void arena_destroy(Arena *arena);

#define ARENA_ALLOC(arena, Type) \
  ((Type *)arena_allocate((arena), sizeof(Type), _Alignof(Type)))

#define ARENA_ALLOC_ARRAY(arena, Type, count) \
  ((Type *)arena_allocate((arena), sizeof(Type) * (count), _Alignof(Type)))

#endif /* SWITCH_COMMON_ARENA_H */
