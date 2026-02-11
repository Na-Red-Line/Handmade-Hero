#include "handmade_tile.h"
#include "handmade_intrinsics.h"
#include "handmade.h"
#include "assert.h"

inline void recanonicalizeCoord(tile_map *tileMap, uint32 *tile, float *tileRel) {
  // 需要找到一种不使用除法/乘法方法来进行重新规范化的方法
  // 因为这种方法最终可能会导致结果四舍五入回到你刚刚离开的格子
  // 假设世界是环形拓扑，如果你从一端走出去，就会从另一端回来

  int32 offset = roundFloatToInt32(*tileRel / (float)tileMap->tileSideInMeters);

  *tile += offset;
  *tileRel -= offset * tileMap->tileSideInMeters;

  // TODO 修复浮点数运算精度问题，使值小于此
  assert(*tileRel >= -0.5 * tileMap->tileSideInMeters);
  assert(*tileRel <= 0.5 * tileMap->tileSideInMeters);
}

inline tile_map_position recanonicalizePosition(tile_map *tileMap, tile_map_position pos) {
  tile_map_position result = pos;

  recanonicalizeCoord(tileMap, &result.absTileX, &result.tileRelX);
  recanonicalizeCoord(tileMap, &result.absTileY, &result.tileRelY);

  return result;
}

inline tile_chunk *getTileChunk(tile_map *tileMap, int32 tileChunkX, int32 tileChunkY, int32 tileChunkZ) {
  tile_chunk *tileChunk = nullptr;

  if ((tileChunkX >= 0 && tileChunkX < tileMap->tileChunkCountX) &&
      (tileChunkY >= 0 && tileChunkY < tileMap->tileChunkCountY)) {
    tileChunk = &tileMap->tileChunks[tileChunkZ * tileChunkX * tileChunkZ +
                                     tileChunkY * tileMap->tileChunkCountX +
                                     tileChunkX];
  }

  return tileChunk;
}

inline uint32 getTileValueUnchecked(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY) {
  assert(tileChunk);
  assert(tileX < tileMap->chunkDim);
  assert(tileY < tileMap->chunkDim);

  uint32 tileChunkValue = tileChunk->tiles[tileY * tileMap->chunkDim + tileX];
  return tileChunkValue;
}

inline void setTileValueUnchecked(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY, uint32 value) {
  assert(tileChunk);
  assert(tileX < tileMap->chunkDim);
  assert(tileY < tileMap->chunkDim);

  tileChunk->tiles[tileY * tileMap->chunkDim + tileX] = value;
}

inline uint32 getTileValue(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY) {
  uint32 tileChunkValue = 0;

  if (tileChunk && tileChunk->tiles) {
    tileChunkValue = getTileValueUnchecked(tileMap, tileChunk, tileX, tileY);
  }

  return tileChunkValue;
}

inline void setTileValue(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY, uint32 value) {
  if (tileChunk) {
    setTileValueUnchecked(tileMap, tileChunk, tileX, tileY, value);
  }
}

inline tile_chunk_position getChunkPositionFor(tile_map *tileMap, uint32 absTileX, uint32 absTileY, uint32 absTileZ) {
  tile_chunk_position result = {};

  result.tileChunkX = absTileX >> tileMap->chunkShift;
  result.tileChunkY = absTileY >> tileMap->chunkShift;
  result.tileChunkZ = absTileZ;
  result.tileX = absTileX & tileMap->chunkMask;
  result.tileY = absTileY & tileMap->chunkMask;

  return result;
}

inline uint32 getTileValue(tile_map *tileMap, uint32 absTileX, uint32 absTileY, uint32 absTileZ) {
  tile_chunk_position chunkPos = getChunkPositionFor(tileMap, absTileX, absTileY, absTileZ);
  tile_chunk *tileChunk = getTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);
  uint32 tileChunkValue = getTileValue(tileMap, tileChunk, chunkPos.tileX, chunkPos.tileY);

  return tileChunkValue;
}

inline bool isTileMapPointEmpty(tile_map *tileMap, tile_map_position pos) {
  bool empty = false;

  uint32 tileChunkValue = getTileValue(tileMap, pos.absTileX, pos.absTileY, pos.absTileZ);
  empty = tileChunkValue == 1;

  return empty;
}

inline void setTileValue(memory_arena *arena, tile_map *tileMap,
                         uint32 absTileX, uint32 absTileY, uint32 absTileZ,
                         uint32 value) {
  tile_chunk_position chunkPos = getChunkPositionFor(tileMap, absTileX, absTileY, absTileZ);
  tile_chunk *tileChunk = getTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);

  assert(tileChunk);
  if (!tileChunk->tiles) {
    uint32 tileCount = tileMap->chunkDim * tileMap->chunkDim;
    tileChunk->tiles = pushArray(arena, tileCount, uint32);
    for (int i = 0; i < tileCount; ++i) {
      tileChunk->tiles[i] = 0;
    }
  }

  setTileValue(tileMap, tileChunk, chunkPos.tileX, chunkPos.tileY, value);
}
