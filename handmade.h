#pragma once

#include "handmade_platform.h"

#include <assert.h>
#include <math.h>

constexpr float PI = 3.14159265359f;

template <typename T, int N>
constexpr int arr_length(T (&)[N]) { return N; }

constexpr uint64 KiloBytes(uint64 x) { return x << 10; }
constexpr uint64 MegaBytes(uint64 x) { return KiloBytes(x) << 10; }
constexpr uint64 GigaBytes(uint64 x) { return MegaBytes(x) << 10; }
constexpr uint64 TeraBytes(uint64 x) { return GigaBytes(x) << 10; }

constexpr uint32 saveCastUint64(uint64 value) {
  assert(value <= 0xffffffff);
  return (uint32)(value);
}

//
//
//

struct memory_arena {
  size_t size;
  uint8 *base;
  size_t used;
};

static void initializerArena(memory_arena *arena, size_t size, uint8 *base) {
  arena->size = size;
  arena->base = base;
  arena->used = 0;
}

#define pushStruct(arena, type) (type *)pushSize_(arena, sizeof(type));
#define pushArray(arena, count, type) (type *)pushSize_(arena, (count) * sizeof(type));

void *pushSize_(memory_arena *arena, size_t size) {
  assert((arena->used + size) <= arena->size);
  void *result = arena->base + arena->used;
  arena->used += size;
  return result;
}

#include "handmade_tile.h"
#include "handmade_intrinsics.h"

struct world {
  tile_map *tileMap;
};

struct game_state {
  memory_arena worldArena;
  world *world;

  tile_map_position playerP;
};
