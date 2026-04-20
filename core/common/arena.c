#include "common/arena.h"
#include "common/assert.h"

#include <stdlib.h>
#include <string.h>

int arena_create(Arena* out, size_t capacity_bytes) {
  SWITCH_ASSERT_ALWAYS(out != NULL, "arena_create: out is NULL");
  void* memory = malloc(capacity_bytes);
  if (!memory) return 0;
  out->base           = (uint8_t*)memory;
  out->capacity_bytes = capacity_bytes;
  out->used_bytes     = 0;
  out->owns_memory    = 1;
  return 1;
}

void arena_create_in_place(Arena* out, void* memory, size_t capacity_bytes) {
  SWITCH_ASSERT_ALWAYS(out != NULL, "arena_create_in_place: out is NULL");
  SWITCH_ASSERT_ALWAYS(memory != NULL, "arena_create_in_place: memory is NULL");
  out->base           = (uint8_t*)memory;
  out->capacity_bytes = capacity_bytes;
  out->used_bytes     = 0;
  out->owns_memory    = 0;
}

static size_t align_up(size_t value, size_t alignment) {
  SWITCH_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0,
                "alignment must be a power of two");
  return (value + (alignment - 1)) & ~(alignment - 1);
}

void* arena_allocate(Arena* arena, size_t size, size_t alignment) {
  SWITCH_ASSERT_ALWAYS(arena != NULL, "arena_allocate: arena is NULL");
  size_t aligned = align_up(arena->used_bytes, alignment);
  if (aligned + size > arena->capacity_bytes) return NULL;
  void* ptr = arena->base + aligned;
  arena->used_bytes = aligned + size;
  return ptr;
}

void arena_reset(Arena* arena) {
  if (!arena) return;
  arena->used_bytes = 0;
}

void arena_destroy(Arena* arena) {
  if (!arena) return;
  if (arena->owns_memory && arena->base) free(arena->base);
  arena->base           = NULL;
  arena->capacity_bytes = 0;
  arena->used_bytes     = 0;
  arena->owns_memory    = 0;
}
