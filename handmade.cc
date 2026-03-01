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

static void drawRectangle(game_offscreen_buffer *buffer, v2 Min, v2 Max, float R, float G, float B) {

  int32 MinX = roundFloatToInt32(Min.X);
  int32 MaxX = roundFloatToInt32(Max.X);
  int32 MinY = roundFloatToInt32(Min.Y);
  int32 MaxY = roundFloatToInt32(Max.Y);

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

static void drawBitmap(game_offscreen_buffer *buffer, loaded_bitmap *bitmap,
                       float realX, float realY,
                       int32 alignX, int32 alignY) {

  realX -= (float)alignX;
  realY -= (float)alignY;

  int32 MinX = roundFloatToInt32(realX);
  int32 MinY = roundFloatToInt32(realY);
  int32 MaxX = MinX + bitmap->width;
  int32 MaxY = MinY + (float)bitmap->height;

  int32 sourceOffsetX = 0;
  if (MinX < 0) {
    sourceOffsetX = -MinX;
    MinX = 0;
  }

  int32 sourceOffsetY = 0;
  if (MinY < 0) {
    sourceOffsetY = -MinY;
    MinY = 0;
  }

  if (MaxX > buffer->width) {
    MaxX = buffer->width;
  }

  if (MaxY > buffer->height) {
    MaxY = buffer->height;
  }

  uint32 *sourceRow = bitmap->pixels + bitmap->width * (bitmap->height - 1);
  sourceRow += -sourceOffsetY * bitmap->width + sourceOffsetX;
  uint8 *destRow = (uint8 *)buffer->memory + MinY * buffer->pitch + MinX * buffer->bytesPerPixel;
  for (int y = MinY; y < MaxY; ++y) {
    uint32 *dest = (uint32 *)destRow;
    uint32 *source = sourceRow;
    for (int x = MinX; x < MaxX; ++x) {

      uint32 S = *source;
      uint32 D = *dest;

      float alpha = (float)((S >> 24) & 0XFF) / 255;
      float SR = (float)((S >> 16) & 0XFF);
      float SG = (float)((S >> 8) & 0XFF);
      float SB = (float)((S >> 0) & 0XFF);

      float DR = (float)((D >> 16) & 0XFF);
      float DG = (float)((D >> 8) & 0XFF);
      float DB = (float)((D >> 0) & 0XFF);

#define MX(C) (D##C - alpha * (D##C - S##C))
      float R = MX(R);
      float G = MX(G);
      float B = MX(B);

      *dest = (((uint32)(R + 0.5f) << 16) |
               ((uint32)(G + 0.5f) << 8) |
               ((uint32)(B + 0.5f) << 0));

      dest++;
      source++;
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

    uint32 redMask = header->redMask;
    uint32 greenMask = header->greenMask;
    uint32 blueMask = header->blueMask;
    uint32 alphaMask = ~(redMask | greenMask | blueMask);

    bit_scan_result redShift = findLeastSignificantSetBit(redMask);
    bit_scan_result greenShift = findLeastSignificantSetBit(greenMask);
    bit_scan_result blueShift = findLeastSignificantSetBit(blueMask);
    bit_scan_result alphaShift = findLeastSignificantSetBit(alphaMask);

    assert(redShift.found);
    assert(greenShift.found);
    assert(blueShift.found);
    assert(alphaShift.found);

    uint32 *sourceDest = pixels;
    for (int y = 0; y < result.height; ++y) {
      for (int x = 0; x < result.width; ++x) {
        // RR GG BB AA -> AA RR GG BB
        uint32 C = *sourceDest;
        *sourceDest++ = ((((C >> alphaShift.index) & 0XFF) << 24) |
                         (((C >> redShift.index) & 0XFF) << 16) |
                         (((C >> greenShift.index) & 0XFF) << 8) |
                         (((C >> blueShift.index) & 0XFF) << 0));
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

    hero_bitmaps *bitmap = gameState->heroBitmaps;

    bitmap->Head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_right_head.bmp");
    bitmap->Cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
    bitmap->Torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
    bitmap->alignX = 72;
    bitmap->alignY = 182;
    bitmap++;

    bitmap->Head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_back_head.bmp");
    bitmap->Cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
    bitmap->Torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
    bitmap->alignX = 72;
    bitmap->alignY = 182;
    bitmap++;

    bitmap->Head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_left_head.bmp");
    bitmap->Cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
    bitmap->Torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
    bitmap->alignX = 72;
    bitmap->alignY = 182;
    bitmap++;

    bitmap->Head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_front_head.bmp");
    bitmap->Cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
    bitmap->Torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
    bitmap->alignX = 72;
    bitmap->alignY = 182;
    bitmap++;

    gameState->CameraP.absTileX = 17 / 2;
    gameState->CameraP.absTileY = 9 / 2;

    gameState->playerP.absTileX = 1;
    gameState->playerP.absTileY = 3;
    gameState->playerP.absTileZ = 0;
    gameState->playerP.offset = {5.0f, 5.0f};

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

          if (tileX == 10 && tileY == 3) {
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
      // 加速度
      v2 ddPlayer = {};

      if (controller.moveUp.endDown) {
        gameState->heroFacingDirection = 1;
        ddPlayer.Y = 1.0f;
      }
      if (controller.moveDown.endDown) {
        gameState->heroFacingDirection = 3;
        ddPlayer.Y = -1.0f;
      }
      if (controller.moveLeft.endDown) {
        gameState->heroFacingDirection = 2;
        ddPlayer.X = -1.0f;
      }
      if (controller.moveRight.endDown) {
        gameState->heroFacingDirection = 0;
        ddPlayer.X = 1.0f;
      }

      if (ddPlayer.X && ddPlayer.Y) {
        ddPlayer *= 0.707106781187f;
      }

      // m/s^2
      float playerSpeed = 5.0f;
      if (controller.actionUp.endDown) {
        playerSpeed = 10.0f;
      }
      ddPlayer *= playerSpeed;
      ddPlayer += -1.5f * gameState->dPlayerP;

      tile_map_position newPlayerP = gameState->playerP;

      // 位置 p = 0.5 * a * t^2 + v * t + p
      newPlayerP.offset = (0.5f * ddPlayer * square(gameInput->dtForFrame) + gameState->dPlayerP * gameInput->dtForFrame + newPlayerP.offset);
      // 速度
      gameState->dPlayerP = ddPlayer * gameInput->dtForFrame + gameState->dPlayerP;
      newPlayerP = recanonicalizePosition(tileMap, newPlayerP);

      tile_map_position playerLeft = newPlayerP;
      playerLeft.offset.X -= 0.5f * playerWidth;
      playerLeft = recanonicalizePosition(tileMap, playerLeft);

      tile_map_position playerRight = newPlayerP;
      playerRight.offset.X += 0.5f * playerWidth;
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
        gameState->CameraP.absTileZ = newPlayerP.absTileZ;
      }

      auto diff = subtract(tileMap, &gameState->playerP, &gameState->CameraP);
      if (diff.dXY.X > (9.0f * tileMap->tileSideInMeters)) {
        gameState->CameraP.absTileX += 17;
      }
      if (diff.dXY.X < -(9.0f * tileMap->tileSideInMeters)) {
        gameState->CameraP.absTileX -= 17;
      }
      if (diff.dXY.Y > (5.0f * tileMap->tileSideInMeters)) {
        gameState->CameraP.absTileY += 9;
      }
      if (diff.dXY.Y < -(5.0f * tileMap->tileSideInMeters)) {
        gameState->CameraP.absTileY -= 9;
      }
    }
  }

  drawBitmap(buffer, &gameState->backdrop, 0, 0, 0, 0);

  float screenCenterX = 0.5f * (float)buffer->width;
  float screenCenterY = 0.5f * (float)buffer->height;

  for (int32 relRow = -10; relRow < 10; ++relRow) {
    for (int32 relColumn = -20; relColumn < 20; ++relColumn) {
      uint32 column = gameState->CameraP.absTileX + relColumn;
      uint32 row = gameState->CameraP.absTileY + relRow;
      uint32 tileID = getTileValue(tileMap, column, row, gameState->CameraP.absTileZ);

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

      v2 tileSid = {(float)(0.5 * tileMap->tileSideInPixels), (float)(0.5 * tileMap->tileSideInPixels)};
      v2 cen = {screenCenterX - tileMap->metersToPixels * gameState->CameraP.offset.X + ((float)relColumn) * tileMap->tileSideInPixels,
                screenCenterY + tileMap->metersToPixels * gameState->CameraP.offset.Y - ((float)relRow) * tileMap->tileSideInPixels};
      v2 Min = cen - tileSid;
      v2 Max = cen + tileSid;

      drawRectangle(buffer, Min, Max, gray, gray, gray);
    }
  }

  auto diff = subtract(tileMap, &gameState->playerP, &gameState->CameraP);

  float playerR = 1.0f;
  float playerG = 1.0f;
  float playerB = 0.0f;
  float playerGroundPointX = screenCenterX + diff.dXY.X * tileMap->metersToPixels;
  float playerGroundPointY = screenCenterY - diff.dXY.Y * tileMap->metersToPixels;

  v2 playerLeftTop = {playerGroundPointX - (float)(0.5 * playerWidth * tileMap->metersToPixels), playerGroundPointY - playerHeight * tileMap->metersToPixels};
  v2 playerWidthHeight = {playerWidth, playerHeight};
  drawRectangle(buffer, playerLeftTop, playerLeftTop + playerWidthHeight * tileMap->metersToPixels, playerR, playerG, playerB);

  hero_bitmaps *heroBitmaps = &gameState->heroBitmaps[gameState->heroFacingDirection];
  drawBitmap(buffer, &heroBitmaps->Head, playerGroundPointX, playerGroundPointY, heroBitmaps->alignX, heroBitmaps->alignY);
  drawBitmap(buffer, &heroBitmaps->Cape, playerGroundPointX, playerGroundPointY, heroBitmaps->alignX, heroBitmaps->alignY);
  drawBitmap(buffer, &heroBitmaps->Torso, playerGroundPointX, playerGroundPointY, heroBitmaps->alignX, heroBitmaps->alignY);
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
  game_state *gameState = (game_state *)memory->permanentStorage;
  game_output_sound(soundOutputBuffer, gameState);
}
