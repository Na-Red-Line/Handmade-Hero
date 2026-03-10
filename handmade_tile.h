#pragma once

#include "handmade_platform.h"
#include "handmade_math.h"

struct tile_map_difference {
  v2 dXY;
  float dZ;
};

struct tile_map_position {
  // 取瓦片地图的 x 和 y 坐标，以及瓦片本身的 x 和 y 坐标
  // 将它们打包成单个32位值，用于 x 和 y，其中低位用于图块索引，高位则是图块的页码

  u32 absTileX;
  u32 absTileY;
  u32 absTileZ;

  v2 offset;
};

struct tile_chunk_position {
  u32 tileChunkX;
  u32 tileChunkY;
  u32 tileChunkZ;

  u32 tileX;
  u32 tileY;
};

struct tile_chunk {
  u32 *tiles;
};

struct tile_map {
  u32 chunkShift;
  u32 chunkMask;
  u32 chunkDim;

  float tileSideInMeters;
  u32 tileSideInPixels;
  float metersToPixels;

  i32 tileChunkCountX;
  i32 tileChunkCountY;
  i32 tileChunkCountZ;

  tile_chunk *tileChunks;
};
