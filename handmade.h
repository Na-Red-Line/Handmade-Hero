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

// 高活跃实体，相对相机的局部坐标位置
struct high_entity {
  v2 P;
  v2 dP;

	u32 absTileZ;
  u32 facingDirection;

	f32 Z;
	f32 dZ;
};

struct low_entity {
};

enum struct Entity_Type : u32
{
	NONE,
	HERO,
	WALL,
};

// 休眠实体，世界坐标
struct dormant_entity
{
	Entity_Type type;

	tile_map_position P;
	f32 width, height;

	bool collides;
	i32 dAbsTileZ;
};

enum struct entity_residence : u32
{
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
  static constexpr u32 entity_count = 256;

  memory_arena worldArena;
  world *world;

  u32 cameraFollowingEntityIndex;
  tile_map_position CameraP;

  u32 playerIndexForController[game_input::controllerCount];
  u32 entityCount;

  entity_residence entityResidence[entity_count];
  high_entity highEntitys[entity_count];
  low_entity lowEntitys[entity_count];
  dormant_entity dormantEntitys[entity_count];

  loaded_bitmap backdrop;
  loaded_bitmap shadow;
  hero_bitmaps heroBitmaps[4];
};
