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

static tile_map *getTileMap(world *world, int32 tileMapX, int32 tileMapY) {
  tile_map *tileMap = nullptr;

  if ((tileMapX >= 0 && tileMapX < world->tileMapCountX) && (tileMapY >= 0 && tileMapY < world->tileMapCountY)) {
    tileMap = &world->tileMaps[tileMapY * world->tileMapCountX + tileMapX];
  }

  return tileMap;
}

static uint32 getTileValueUnchecked(world *world, tile_map *tileMap, int tileX, int tileY) {
  assert(tileMap);
  assert((tileX >= 0 && (tileX <= world->countX)) && (tileY >= 0 && (tileY <= world->countY)));

  uint32 result = tileMap->tiles[world->countX * tileY + tileX];
  return result;
}

static bool isTileMapPointEmpty(world *world, tile_map *tileMap, int32 tileX, int32 tileY) {
  bool empty = false;

  if (!tileMap) {
    return empty;
  }

  if ((tileX >= 0 && (tileX <= world->countX)) && (tileY >= 0 && (tileY <= world->countY))) {
    uint32 tileMapValue = getTileValueUnchecked(world, tileMap, tileX, tileY);
    empty = tileMapValue == 0;
  }

  return empty;
}

static canonical_position getCanonicalPosition(world *world, raw_position pos) {
  canonical_position result = {};

  result.tileMapX = pos.tileMapX;
  result.tileMapY = pos.tileMapY;

  float X = pos.X - world->upperLeftX;
  float Y = pos.Y - world->upperLeftY;
  result.tileX = floorfToInt32(X / world->tileWidth);
  result.tileY = floorfToInt32(Y / world->tileHeight);

  result.tileRelX = X - result.tileX * world->tileWidth;
  result.tileRelY = Y - result.tileY * world->tileHeight;

  assert(result.tileRelX >= 0);
  assert(result.tileRelY >= 0);
  assert(result.tileRelX < world->tileWidth);
  assert(result.tileRelX < world->tileHeight);

  if (result.tileX < 0) {
    result.tileMapX--;
    result.tileX = world->countX + result.tileX;
  }

  if (result.tileX >= world->countX) {
    result.tileMapX++;
    result.tileX = result.tileX - world->countX;
  }

  if (result.tileY < 0) {
    result.tileMapY--;
    result.tileY = world->countY + result.tileY;
  }

  if (result.tileY >= world->countY) {
    result.tileMapY++;
    result.tileY = result.tileY - world->countY;
  }

  return result;
}

static bool isWordPointEmpty(world *world, raw_position pos) {
  bool empty = false;

  canonical_position canPos = getCanonicalPosition(world, pos);
  tile_map *tileMap = getTileMap(world, canPos.tileMapX, canPos.tileMapY);
  empty = isTileMapPointEmpty(world, tileMap, canPos.tileX, canPos.tileY);

  return empty;
}

extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
  static bool isWrite;

  assert(&gameInput->controller->terminator - gameInput->controller->Button == arr_length(gameInput->controller->Button));
  assert(sizeof(game_state) <= memory->permanentStorageSize);

  constexpr uint32 TILE_MAP_COUNT_X = 17;
  constexpr uint32 TILE_MAP_COUNT_Y = 9;

  uint32 tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  uint32 tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  uint32 tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  uint32 tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  tile_map tileMaps[2][2] = {};
  tileMaps[0][0].tiles = (uint32 *)tiles00;
  tileMaps[0][1].tiles = (uint32 *)tiles10;
  tileMaps[1][0].tiles = (uint32 *)tiles01;
  tileMaps[1][1].tiles = (uint32 *)tiles11;

  world world = {};

  world.countX = TILE_MAP_COUNT_X;
  world.countY = TILE_MAP_COUNT_Y;
  world.upperLeftX = -10;
  world.upperLeftY = 0;
  world.tileWidth = 80;
  world.tileHeight = 80;

  world.tileMapCountX = 2;
  world.tileMapCountY = 2;
  world.tileMaps = (tile_map *)tileMaps;

  float playerWidth = 0.75 * world.tileWidth;
  float playerHeight = world.tileHeight;

  game_state *gameState = (game_state *)memory->permanentStorage;

  tile_map *tileMap = getTileMap(&world, gameState->playerTileMapX, gameState->playerTileMapY);
  assert(tileMap);

  if (!memory->isInitialized) {

    gameState->playerX = 200;
    gameState->playerY = 200;

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
        dPlayerY = -1.0f;
      }
      if (controller.moveDown.endDown) {
        dPlayerY = 1.0f;
      }
      if (controller.moveLeft.endDown) {
        dPlayerX = -1.0f;
      }
      if (controller.moveRight.endDown) {
        dPlayerX = 1.0f;
      }
      dPlayerX *= 120.0f;
      dPlayerY *= 120.0f;

      float newPlayX = gameState->playerX + gameInput->dtForFrame * dPlayerX;
      float newPlayY = gameState->playerY + gameInput->dtForFrame * dPlayerY;

      raw_position playerPos = {};
      playerPos.tileMapX = gameState->playerTileMapX;
      playerPos.tileMapY = gameState->playerTileMapY;
      playerPos.X = newPlayX;
      playerPos.Y = newPlayY;

      raw_position playerLeft = playerPos;
      playerLeft.X -= 0.5f * playerWidth;

      raw_position playerRight = playerPos;
      playerRight.X += 0.5f * playerWidth;

      if (isWordPointEmpty(&world, playerPos) &&
          isWordPointEmpty(&world, playerLeft) &&
          isWordPointEmpty(&world, playerRight)) {

        canonical_position canPos = getCanonicalPosition(&world, playerPos);

        gameState->playerTileMapX = canPos.tileMapX;
        gameState->playerTileMapY = canPos.tileMapY;
        gameState->playerX = world.upperLeftX + canPos.tileX * world.tileWidth + canPos.tileRelX;
        gameState->playerY = world.upperLeftY + canPos.tileY * world.tileHeight + canPos.tileRelY;
      }
    }
  }

  drawRectangle(buffer, 0.0f, (float)buffer->width, 0.0f, (float)buffer->height, 1.0f, 0.0f, 1.0f);

  for (int row = 0; row < TILE_MAP_COUNT_Y; ++row) {
    for (int column = 0; column < TILE_MAP_COUNT_X; ++column) {
      uint32 tileID = getTileValueUnchecked(&world, tileMap, column, row);
      float gray = 0.5f;
      if (tileID == 1) {
        gray = 1.0f;
      }

      float MinX = world.upperLeftX + ((float)column) * world.tileWidth;
      float MaxX = MinX + world.tileWidth;
      float MinY = world.upperLeftY + ((float)row) * world.tileHeight;
      float MaxY = MinY + world.tileHeight;

      drawRectangle(buffer, MinX, MaxX, MinY, MaxY, gray, gray, gray);
    }
  }

  float playerR = 1.0f;
  float playerG = 1.0f;
  float playerB = 0.0f;
  float playerLeft = gameState->playerX - 0.5 * playerWidth;
  float playerTop = gameState->playerY - playerHeight;
  drawRectangle(buffer,
                playerLeft, playerLeft + playerWidth,
                playerTop, playerTop + playerHeight,
                playerR, playerG, playerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
  game_state *gameState = (game_state *)memory->permanentStorage;
  game_output_sound(soundOutputBuffer, gameState);
}
