#pragma once

#include "handmade_platform.h"

struct tile_map_position {
  // 取瓦片地图的 x 和 y 坐标，以及瓦片本身的 x 和 y 坐标
  // 将它们打包成单个32位值，用于 x 和 y，其中低位用于图块索引，高位则是图块的页码

  uint32 absTileX;
  uint32 absTileY;

  float tileRelX;
  float tileRelY;
};

struct tile_chunk_position {
  uint32 tileChunkX;
  uint32 tileChunkY;

  uint32 tileX;
  uint32 tileY;
};

struct tile_chunk {
  uint32 *tiles;
};

struct tile_map {
  uint32 chunkShift;
  uint32 chunkMask;
  uint32 chunkDim;

  float tileSideInMeters;
  uint32 tileSideInPixels;
  float metersToPixels;

  int32 tileChunkCountX;
  int32 tileChunkCountY;

  tile_chunk *tileChunks;
};
