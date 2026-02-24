#include "handmade.h"
#include "handmade_random.h"
#include "handmade_tile.cc"

#include <stdio.h>

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

static void drawRectangle(game_offscreen_buffer *buffer,
                          float rectMinX, float rectMaxX,
                          float rectMinY, float rectMaxY,
                          float R, float G, float B) {

  int32 MinX = roundFloatToInt32(rectMinX);
  int32 MaxX = roundFloatToInt32(rectMaxX);
  int32 MinY = roundFloatToInt32(rectMinY);
  int32 MaxY = roundFloatToInt32(rectMaxY);

  if (MinX < 0) MinX = 0;
  if (MinY < 0) MinY = 0;
  if (MaxX > buffer->width) MaxX = buffer->width;
  if (MaxY > buffer->height) MaxY = buffer->height;

  uint32 color = (roundFloatToUInt32(R * 255.0f) << 16) |
                 (roundFloatToUInt32(G * 255.0f) << 8) |
                 (roundFloatToUInt32(B * 255.0f) << 0);

  uint8 *row = (uint8 *)buffer->memory + MinY * buffer->pitch + MinX * buffer->bytesPerPixel;
  for (int y = MinY; y < MaxY; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = MinX; x < MaxX; ++x) {
      *pixel++ = color;
    }
    row += buffer->pitch;
  }
}

static void drawBitmap(game_offscreen_buffer *buffer, loaded_bitmap *bitmap, float realX, float realY) {
  int32 MinX = roundFloatToInt32(realX);
  int32 MinY = roundFloatToInt32(realY);
  int32 MaxX = roundFloatToInt32(realX + bitmap->width);
  int32 MaxY = roundFloatToInt32(realY + bitmap->height);

  if (MinX < 0) MinX = 0;
  if (MinY < 0) MinY = 0;
  if (MaxX > buffer->width) MaxX = buffer->width;
  if (MaxY > buffer->height) MaxY = buffer->height;

  uint32 *sourceRow = bitmap->pixels + bitmap->width * (bitmap->height - 1);
  // bitmap 数据从上到下
  uint8 *destRow = (uint8 *)buffer->memory + MinY * buffer->pitch + MinX * buffer->bytesPerPixel;
  for (int y = MinY; y < MaxY; ++y) {
    uint32 *dest = (uint32 *)destRow;
    uint32 *source = sourceRow;
    for (int x = MinX; x < MaxX; ++x) {
      *dest++ = *source++;
    }
    destRow += buffer->pitch;
    sourceRow -= bitmap->width;
  }
}

// https://www.cnblogs.com/Matrix_Yao/archive/2009/12/02/1615295.html
// bmp 文件格式
#pragma pack(push, 1)
struct bitmap_header {
  uint16 fileType;
  uint32 fileSize;
  uint16 reserved1;
  uint16 reserved2;
  uint32 bitmapOffset;
  uint32 size;
  int32 width;
  int32 height;
  uint16 planes;
  uint16 bitsPerPixel;
  uint32 compression;
  uint32 sizeOfBitmap;
  int32 horzResolution;
  int32 vertResolution;
  uint32 colorsUsed;
  uint32 colorsImportant;

  uint32 redMask;
  uint32 greenMask;
  uint32 blueMask;
};
#pragma pack(pop)

static loaded_bitmap DEBUGLoadBMP(thread_context *thread, debug_platform_read_entire_file *readEntireFile, const char *fileName) {
  loaded_bitmap result = {};

  debug_read_file_result readFileResult = readEntireFile(thread, fileName);
  if (readFileResult.fileSize != 0) {
    bitmap_header *header = (bitmap_header *)readFileResult.contents;
    uint32 *pixels = (uint32 *)((uint8 *)header + header->bitmapOffset);

    result.pixels = pixels;
    result.width = header->width;
    result.height = header->height;

    uint32 *sourceDest = pixels;
    for (int y = 0; y < result.height; ++y) {
      for (int x = 0; x < result.width; ++x) {
        // RR GG BB AA -> AA RR GG BB
        *sourceDest = (*sourceDest >> 8) | (*sourceDest << 24);
        ++sourceDest;
      }
    }
  }

  return result;
}

extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
  static bool isWrite;

  assert(&gameInput->controller->terminator - gameInput->controller->Button == arr_length(gameInput->controller->Button));
  assert(sizeof(game_state) <= memory->permanentStorageSize);

  float playerHeight = 1.4f;
  float playerWidth = 0.75 * playerHeight;

  game_state *gameState = (game_state *)memory->permanentStorage;
  if (!memory->isInitialized) {
    gameState->backdrop = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_background.bmp");
    gameState->heroHead = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_front_head.bmp");
    gameState->heroCape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
    gameState->heroTorso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_front_torso.bmp");

    gameState->playerP.absTileX = 1;
    gameState->playerP.absTileY = 3;
    gameState->playerP.absTileZ = 0;
    gameState->playerP.tileRelX = 5.0f;
    gameState->playerP.tileRelY = 5.0f;

    initializerArena(&gameState->worldArena,
                     memory->permanentStorageSize - sizeof(game_state),
                     (uint8 *)memory->permanentStorage + sizeof(game_state));

    gameState->world = pushStruct(&gameState->worldArena, world);
    world *world = gameState->world;
    world->tileMap = pushStruct(&gameState->worldArena, tile_map);

    tile_map *tileMap = world->tileMap;

    tileMap->chunkShift = 4;
    tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
    tileMap->chunkDim = (1 << tileMap->chunkShift);

    tileMap->tileSideInMeters = 1.4f;
    tileMap->tileSideInPixels = 60;
    tileMap->metersToPixels = (float)tileMap->tileSideInPixels / (float)tileMap->tileSideInMeters;

    tileMap->tileChunkCountX = 128;
    tileMap->tileChunkCountY = 128;
    tileMap->tileChunkCountZ = 2;
    tileMap->tileChunks = pushArray(&gameState->worldArena,
                                    tileMap->tileChunkCountX * tileMap->tileChunkCountY * tileMap->tileChunkCountZ,
                                    tile_chunk);

    uint32 tilesPerWidth = 17;
    uint32 tilesPerHeight = 9;
    uint32 screenX = 0;
    uint32 screenY = 0;
    uint32 absTileZ = 0;

    bool doorLeft = false;
    bool doorRight = false;
    bool doorTop = false;
    bool doorBottom = false;
    bool doorUp = false;
    bool doorDown = false;

    for (int screenIndex = 0; screenIndex < 50; ++screenIndex) {
      int randomChoice = 0;
      if (doorUp || doorDown) {
        randomChoice = random() % 2;
      } else {
        randomChoice = random() % 3;
      }

      if (randomChoice == 2) {
        if (absTileZ == 0) {
          doorUp = true;
        } else {
          doorDown = true;
        }
      } else if (randomChoice == 1) {
        doorLeft = true;
      } else {
        doorTop = true;
      }

      for (uint32 tileY = 0; tileY < tilesPerHeight; ++tileY) {
        for (uint32 tileX = 0; tileX < tilesPerWidth; ++tileX) {
          uint32 absTileX = (screenX * tilesPerWidth) + tileX;
          uint32 absTileY = (screenY * tilesPerHeight) + tileY;

          int value = 1;

          if (tileY == 0 && (!doorBottom || tileX != 8)) {
            value = 2;
          }

          if (tileY == 8 && (!doorTop || tileX != 8)) {
            value = 2;
          }

          if (tileX == 0 && (!doorRight || tileY != 4)) {
            value = 2;
          }

          if (tileX == 16 && (!doorLeft || tileY != 4)) {
            value = 2;
          }

          if (tileX == 8 && tileY == 4) {
            if (doorUp) {
              value = 3;
            }

            if (doorDown) {
              value = 4;
            }
          }

          setTileValue(
              &gameState->worldArena, world->tileMap,
              absTileX, absTileY, absTileZ,
              value);
        }
      }

      doorRight = doorLeft;
      doorBottom = doorTop;

      if (randomChoice == 2) {
        if (doorUp) {
          doorDown = true;
          doorUp = false;
        } else if (doorDown) {
          doorUp = true;
          doorDown = false;
        }
      } else {
        doorUp = false;
        doorDown = false;
      }

      doorLeft = false;
      doorTop = false;

      if (randomChoice == 2) {
        if (absTileZ == 0) {
          absTileZ = 1;
        } else {
          absTileZ = 0;
        }
      } else if (randomChoice == 1) {
        screenX += 1;
      } else {
        screenY += 1;
      }
    }

    memory->isInitialized = true;
  }

  world *world = gameState->world;
  tile_map *tileMap = world->tileMap;

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

      float playerSpeed = 2.0f;
      if (controller.actionUp.endDown) {
        playerSpeed = 10.0f;
      }

      dPlayerX *= playerSpeed;
      dPlayerY *= playerSpeed;

      tile_map_position newPlayerP = gameState->playerP;
      newPlayerP.tileRelX += gameInput->dtForFrame * dPlayerX;
      newPlayerP.tileRelY += gameInput->dtForFrame * dPlayerY;
      newPlayerP = recanonicalizePosition(tileMap, newPlayerP);

      tile_map_position playerLeft = newPlayerP;
      playerLeft.tileRelX -= 0.5f * playerWidth;
      playerLeft = recanonicalizePosition(tileMap, playerLeft);

      tile_map_position playerRight = newPlayerP;
      playerRight.tileRelX += 0.5f * playerWidth;
      playerRight = recanonicalizePosition(tileMap, playerRight);

      if (isTileMapPointEmpty(tileMap, newPlayerP) &&
          isTileMapPointEmpty(tileMap, playerLeft) &&
          isTileMapPointEmpty(tileMap, playerRight)) {

        if (!areOnSameTile(&gameState->playerP, &newPlayerP)) {
          uint32 tileValue = getTileValue(world->tileMap, newPlayerP);
          if (tileValue == 3) {
            newPlayerP.absTileZ += 1;
          }
          if (tileValue == 4) {
            newPlayerP.absTileZ -= 1;
          }
        }

        gameState->playerP = newPlayerP;
      }
    }
  }

  drawRectangle(buffer, 0.0f, (float)buffer->width, 0.0f, (float)buffer->height, 1.0f, 0.0f, 1.0f);
  drawBitmap(buffer, &gameState->backdrop, 0, 0);

  float screenCenterX = 0.5f * (float)buffer->width;
  float screenCenterY = 0.5f * (float)buffer->height;

  for (int32 relRow = -10; relRow < 10; ++relRow) {
    for (int32 relColumn = -20; relColumn < 20; ++relColumn) {
      uint32 column = gameState->playerP.absTileX + relColumn;
      uint32 row = gameState->playerP.absTileY + relRow;
      uint32 tileID = getTileValue(tileMap, column, row, gameState->playerP.absTileZ);

      // 零表示空
      if (tileID < 2) {
        continue;
      }

      float gray = 0.5f;
      if (tileID == 2) {
        gray = 1.0f;
      }

      if (tileID > 2) {
        gray = 0.25f;
      }

      if (column == gameState->playerP.absTileX && row == gameState->playerP.absTileY) {
        gray = 0.0f;
      }

      float cenX = screenCenterX - tileMap->metersToPixels * gameState->playerP.tileRelX +
                   ((float)relColumn) * tileMap->tileSideInPixels;
      float cenY = screenCenterY + tileMap->metersToPixels * gameState->playerP.tileRelY -
                   ((float)relRow) * tileMap->tileSideInPixels;
      float MinX = cenX - 0.5f * tileMap->tileSideInPixels;
      float MaxX = cenX + 0.5f * tileMap->tileSideInPixels;
      float MinY = cenY - 0.5f * tileMap->tileSideInPixels;
      float MaxY = cenY + 0.5f * tileMap->tileSideInPixels;

      drawRectangle(buffer, MinX, MaxX, MinY, MaxY, gray, gray, gray);
    }
  }

  float playerR = 1.0f;
  float playerG = 1.0f;
  float playerB = 0.0f;
  float playerLeft = screenCenterX - 0.5 * playerWidth * tileMap->metersToPixels;
  float playerTop = screenCenterY - playerHeight * tileMap->metersToPixels;
  drawRectangle(buffer,
                playerLeft, playerLeft + playerWidth * tileMap->metersToPixels,
                playerTop, playerTop + playerHeight * tileMap->metersToPixels,
                playerR, playerG, playerB);

  drawBitmap(buffer, &gameState->heroHead, playerLeft, playerTop);
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
  game_state *gameState = (game_state *)memory->permanentStorage;
  game_output_sound(soundOutputBuffer, gameState);
}
