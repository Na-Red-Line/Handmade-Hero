#pragma once

#include "handmade_platform.h"
#include "handmade_math.h"

#include <assert.h>
#include <math.h>

constexpr f32 PI = 3.14159265359f;

template <typename T, u32 N>
constexpr u32 arr_length(T (&)[N]) { return N; }

template <typename T>
constexpr T minimum(const T &a, const T &b) {
  return a < b ? a : b;
}

template <typename T>
constexpr T maximum(const T &a, const T &b) {
  return a > b ? a : b;
}

constexpr u64 KiloBytes(u64 x) { return x << 10; }
constexpr u64 MegaBytes(u64 x) { return KiloBytes(x) << 10; }
constexpr u64 GigaBytes(u64 x) { return MegaBytes(x) << 10; }
constexpr u64 TeraBytes(u64 x) { return GigaBytes(x) << 10; }

constexpr u32 saveCastUint64(u64 value) {
  assert(value <= 0xffffffff);
  return (u32)(value);
}

//
//
//

struct memory_arena {
  size_t size;
  u8 *base;
  size_t used;
};

static void initializerArena(memory_arena *arena, size_t size, u8 *base) {
  arena->size = size;
  arena->base = base;
  arena->used = 0;
}

#define pushStruct(arena, type) (type *)pushSize_(arena, sizeof(type));
#define pushArray(arena, count, type) (type *)pushSize_(arena, (count) * sizeof(type));

inline void *pushSize_(memory_arena *arena, size_t size) {
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

struct loaded_bitmap {
  i32 width;
  i32 height;
  u32 *pixels;
};

struct hero_bitmaps {
  int alignX;
  int alignY;

  loaded_bitmap Head;
  loaded_bitmap Cape;
  loaded_bitmap Torso;
};

struct high_entity {
  bool exists;
  v2 P;
  v2 dP;
  u32 facingDirection;
};

struct low_entity {
};

struct dormant_entity {
  tile_map_position P;
  f32 width, height;
};

enum struct entity_residence : u32 {
  NONEXISTENT,
  DORMANT,
  LOW,
  HIGH,
};

struct entity {
  entity_residence residence;
  low_entity *low;
  dormant_entity *dormant;
  high_entity *high;
};

struct game_state {
  static constexpr u32 ENTITY_COUNT = 256;

  memory_arena worldArena;
  world *world;

  u32 cameraFollowingEntityIndex;
  tile_map_position CameraP;

  u32 playerIndexForController[game_input::controllerCount];
  u32 entityCount;

  entity_residence entityResidence[ENTITY_COUNT];
  high_entity highEntitys[ENTITY_COUNT];
  low_entity lowEntitys[ENTITY_COUNT];
  dormant_entity dormantEntitys[ENTITY_COUNT];

  loaded_bitmap backdrop;
  hero_bitmaps heroBitmaps[4];
};
