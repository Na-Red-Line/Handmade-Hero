#include "handmade.h"

static void game_output_sound(game_sound_output_buffer soundOutputBuffer, game_state *gameState) {
  int16 *sampleOut = (int16 *)soundOutputBuffer.buffer;

#if 0
  float wavePeroid = (float)soundOutputBuffer.samplesPerSecond / (float)gameState->toneHz;
#endif

  for (int sampleIndex = 0; sampleIndex < soundOutputBuffer.bufferSize; ++sampleIndex) {
#if 0
		float sineValue = sinf(gameState->tSine); // 余弦y轴坐标
    int sampleValue = sineValue * soundOutputBuffer.toneVolume;
#endif

    int sampleValue = 0;
    // 写入左右双声道
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;

#if 0
		// 适应频率改变
    gameState->tSine += (PI * 2.0f) / wavePeroid;
    // 防止精度丢失
    if (gameState->tSine > PI * 2.0f) gameState->tSine -= PI * 2.0f;
#endif
  }
}

#if 0
static void renderWeirGradient(game_offscreen_buffer offscreenBuffer, int blueOffset, int greenOffset) {
  uint8 *row = (uint8 *)offscreenBuffer.memory;
  int pitch = offscreenBuffer.width * offscreenBuffer.bytesPerPixel;

  for (int y = 0; y < offscreenBuffer.height; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = 0; x < offscreenBuffer.width; ++x) {
      // memory order: RR GG BB xx
      // loaded in:    xx BB GG RR
      // window:       xx RR GG BB
      // memory order: BB GG RR xx
      uint8 blue = x + blueOffset;
      uint8 green = y + greenOffset;
      *pixel++ = ((green << 8) | blue);
    }
    row += pitch;
  }
}
#endif

template <typename T>
static T roundFloatToInt(float f32) {
  T result = T(f32 + .5f);
  return result;
};

static int32 floorfToInt32(float f32) {
  int32 result = (int32)floorf(f32);
  return result;
}

static void drawRectangle(game_offscreen_buffer *buffer,
                          float rectMinX, float rectMaxX,
                          float rectMinY, float rectMaxY,
                          float R, float G, float B) {

  int32 MinX = roundFloatToInt<int32>(rectMinX);
  int32 MaxX = roundFloatToInt<int32>(rectMaxX);
  int32 MinY = roundFloatToInt<int32>(rectMinY);
  int32 MaxY = roundFloatToInt<int32>(rectMaxY);

  if (MinX < 0) MinX = 0;
  if (MinY < 0) MinY = 0;
  if (MaxX > buffer->width) MaxX = buffer->width;
  if (MaxY > buffer->height) MaxY = buffer->height;

  uint32 color = (roundFloatToInt<uint32>(R * 255.0f) << 16) |
                 (roundFloatToInt<uint32>(G * 255.0f) << 8) |
                 (roundFloatToInt<uint32>(B * 255.0f) << 0);

  uint8 *row = (uint8 *)buffer->memory + MinY * buffer->pitch + MinX * buffer->bytesPerPixel;
  for (int y = MinY; y < MaxY; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = MinX; x < MaxX; ++x) {
      *pixel++ = color;
    }
    row += buffer->pitch;
  }
}

static tile_chunk *getTileChunk(world *world, int32 tileChunkX, int32 tileChunkY) {
  tile_chunk *tileChunk = nullptr;

  if ((tileChunkX >= 0 && tileChunkX < world->tileChunkCountX) &&
      (tileChunkY >= 0 && tileChunkY < world->tileChunkCountY)) {
    tileChunk = &world->tileChunks[tileChunkY * world->tileChunkCountX + tileChunkX];
  }

  return tileChunk;
}

static uint32 getTileValueUnchecked(world *world, tile_chunk *tileChunk, uint32 tileX, uint32 tileY) {
  assert(tileChunk);
  assert(tileX < world->chunkDim);
  assert(tileY < world->chunkDim);

  uint32 tileChunkValue = tileChunk->tiles[tileY * world->chunkDim + tileX];
  return tileChunkValue;
}

static uint32 getTileValue(world *world, tile_chunk *tileChunk, uint32 tileX, uint32 tileY) {
  uint32 tileChunkValue = 0;

  if (tileChunk) {
    tileChunkValue = getTileValueUnchecked(world, tileChunk, tileX, tileY);
  }

  return tileChunkValue;
}

static void recanonicalizeCoord(world *world, uint32 *tile, float *tileRel) {
  // 需要找到一种不使用除法/乘法方法来进行重新规范化的方法
  // 因为这种方法最终可能会导致结果四舍五入回到你刚刚离开的格子
  // 假设世界是环形拓扑，如果你从一端走出去，就会从另一端回来

  int32 offset = floorfToInt32(*tileRel / (float)world->tileSideInMeters);

  *tile += offset;
  *tileRel -= offset * world->tileSideInMeters;

  assert(*tileRel >= 0);
  // TODO 修复浮点数运算精度问题，使值小于此
  assert(*tileRel <= world->tileSideInMeters);
}

static world_position recanonicalizePosition(world *world, world_position pos) {
  world_position result = pos;

  recanonicalizeCoord(world, &result.absTileX, &result.tileRelX);
  recanonicalizeCoord(world, &result.absTileY, &result.tileRelY);

  return result;
}

static tile_chunk_position getChunkPositionFor(world *world, uint32 absTileX, uint32 absTileY) {
  tile_chunk_position result = {};

  result.tileChunkX = absTileX >> world->chunkShift;
  result.tileChunkY = absTileY >> world->chunkShift;
  result.tileX = absTileX & world->chunkMask;
  result.tileY = absTileY & world->chunkMask;

  return result;
}

static uint32 getTileValue(world *world, uint32 absTileX, uint32 absTileY) {
  tile_chunk_position chunkPos = getChunkPositionFor(world, absTileX, absTileY);
  tile_chunk *tileChunk = getTileChunk(world, chunkPos.tileChunkX, chunkPos.tileChunkY);
  uint32 tileChunkValue = getTileValue(world, tileChunk, chunkPos.tileX, chunkPos.tileY);

  return tileChunkValue;
}

static bool isWordPointEmpty(world *world, world_position pos) {
  bool empty = false;

  uint32 tileChunkValue = getTileValue(world, pos.absTileX, pos.absTileY);
  empty = tileChunkValue == 0;

  return empty;
}

extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
  static bool isWrite;

  assert(&gameInput->controller->terminator - gameInput->controller->Button == arr_length(gameInput->controller->Button));
  assert(sizeof(game_state) <= memory->permanentStorageSize);

  constexpr uint32 TILE_MAP_COUNT_X = 256;
  constexpr uint32 TILE_MAP_COUNT_Y = 256;

  uint32 tempTiles[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  world world = {};

  world.chunkShift = 8;
  world.chunkMask = (1 << world.chunkShift) - 1;
  world.chunkDim = 256;

  world.tileChunkCountX = TILE_MAP_COUNT_X;
  world.tileChunkCountY = TILE_MAP_COUNT_Y;

  world.tileSideInMeters = 1.4f;
  world.tileSideInPixels = 60;
  world.metersToPixels = (float)world.tileSideInPixels / world.tileSideInMeters;

  tile_chunk tileChunk = {};
  tileChunk.tiles = (uint32 *)tempTiles;
  world.tileChunks = &tileChunk;

  float playerHeight = 1.4f;
  float playerWidth = 0.75 * playerHeight;

  // NOTE ???
  float lowerLeftX = -(float)world.tileSideInPixels / 2;
  float lowerLeftY = (float)buffer->height / 2;

  game_state *gameState = (game_state *)memory->permanentStorage;
  if (!memory->isInitialized) {

    gameState->playerP.absTileX = 3;
    gameState->playerP.absTileY = 3;
    gameState->playerP.tileRelX = 5.0f;
    gameState->playerP.tileRelY = 5.0f;

    memory->isInitialized = true;
  }

  for (auto &controller : gameInput->controller) {
    if (controller.isAnalog) {
      // NOTE Use analog movement tuning
    } else {
      // NOTE Use digital movement tuning
      float dPlayerX = 0.0f;
      float dPlayerY = 0.0f;

      if (controller.moveUp.endDown) {
        dPlayerY = 1.0f;
      }
      if (controller.moveDown.endDown) {
        dPlayerY = -1.0f;
      }
      if (controller.moveLeft.endDown) {
        dPlayerX = -1.0f;
      }
      if (controller.moveRight.endDown) {
        dPlayerX = 1.0f;
      }
      dPlayerX *= 4.0f;
      dPlayerY *= 4.0f;

      world_position newPlayerP = gameState->playerP;
      newPlayerP.tileRelX += gameInput->dtForFrame * dPlayerX;
      newPlayerP.tileRelY += gameInput->dtForFrame * dPlayerY;
      newPlayerP = recanonicalizePosition(&world, newPlayerP);

      world_position playerLeft = newPlayerP;
      playerLeft.tileRelX -= 0.5f * playerWidth;
      playerLeft = recanonicalizePosition(&world, playerLeft);

      world_position playerRight = newPlayerP;
      playerRight.tileRelX += 0.5f * playerWidth;
      playerRight = recanonicalizePosition(&world, playerRight);

      if (isWordPointEmpty(&world, newPlayerP) &&
          isWordPointEmpty(&world, playerLeft) &&
          isWordPointEmpty(&world, playerRight)) {
        gameState->playerP = newPlayerP;
      }
    }
  }

  drawRectangle(buffer, 0.0f, (float)buffer->width, 0.0f, (float)buffer->height, 1.0f, 0.0f, 1.0f);

  float centerX = 0.5f * (float)buffer->width;
  float centerY = 0.5f * (float)buffer->height;

  for (int32 relRow = -10; relRow < 10; ++relRow) {
    for (int32 relColumn = -20; relColumn < 20; ++relColumn) {
      uint32 column = gameState->playerP.absTileX + relColumn;
      uint32 row = gameState->playerP.absTileY + relRow;
      uint32 tileID = getTileValue(&world, column, row);

      float gray = 0.5f;
      if (tileID == 1) {
        gray = 1.0f;
      }

      if (column == gameState->playerP.absTileX && row == gameState->playerP.absTileY) {
        gray = 0.0f;
      }

      float MinX = centerX + ((float)relColumn) * world.tileSideInPixels;
      float MaxX = MinX + world.tileSideInPixels;
      // MinY实际比MaxY大，是为了图形为Y轴升高
      float MinY = centerY - ((float)relRow) * world.tileSideInPixels;
      float MaxY = MinY - world.tileSideInPixels;

      drawRectangle(buffer, MinX, MaxX, MaxY, MaxY, gray, gray, gray);
    }
  }

  float playerR = 1.0f;
  float playerG = 1.0f;
  float playerB = 0.0f;
  float playerLeft = centerX + (gameState->playerP.tileRelX - 0.5 * playerWidth) * world.metersToPixels;
  float playerTop = centerY - (gameState->playerP.tileRelY - playerHeight) * world.metersToPixels;
  drawRectangle(buffer,
                playerLeft, playerLeft + playerWidth * world.metersToPixels,
                playerTop, playerTop + playerHeight * world.metersToPixels,
                playerR, playerG, playerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
  game_state *gameState = (game_state *)memory->permanentStorage;
  game_output_sound(soundOutputBuffer, gameState);
}
