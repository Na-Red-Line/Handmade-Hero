#include "handmade.h"
#include "handmade_random.h"
#include "handmade_tile.cc"

#include <stdio.h>

static void game_output_sound(game_sound_output_buffer soundOutputBuffer, game_state *gameState) {
  i16 *sampleOut = (i16 *)soundOutputBuffer.buffer;

#if 0
  f32 wavePeroid = (f32)soundOutputBuffer.samplesPerSecond / (f32)gameState->toneHz;
#endif

  for (int sampleIndex = 0; sampleIndex < soundOutputBuffer.bufferSize; ++sampleIndex) {
#if 0
		f32 sineValue = sinf(gameState->tSine); // 余弦y轴坐标
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

static void drawRectangle(game_offscreen_buffer *buffer, v2 Min, v2 Max, f32 R, f32 G, f32 B) {

  i32 MinX = roundFloatToI32(Min.X);
  i32 MaxX = roundFloatToI32(Max.X);
  i32 MinY = roundFloatToI32(Min.Y);
  i32 MaxY = roundFloatToI32(Max.Y);

  if (MinX < 0) MinX = 0;
  if (MinY < 0) MinY = 0;
  if (MaxX > buffer->width) MaxX = buffer->width;
  if (MaxY > buffer->height) MaxY = buffer->height;

  u32 color = (roundFloatToU32(R * 255.0f) << 16) |
              (roundFloatToU32(G * 255.0f) << 8) |
              (roundFloatToU32(B * 255.0f) << 0);

  u8 *row = (u8 *)buffer->memory + MinY * buffer->pitch + MinX * buffer->bytesPerPixel;
  for (int y = MinY; y < MaxY; ++y) {
    u32 *pixel = (u32 *)row;
    for (int x = MinX; x < MaxX; ++x) {
      *pixel++ = color;
    }
    row += buffer->pitch;
  }
}

static void drawBitmap(game_offscreen_buffer *buffer, loaded_bitmap *bitmap,
                       f32 realX, f32 realY,
                       i32 alignX, i32 alignY) {

  realX -= (f32)alignX;
  realY -= (f32)alignY;

  i32 MinX = roundFloatToI32(realX);
  i32 MinY = roundFloatToI32(realY);
  i32 MaxX = MinX + bitmap->width;
  i32 MaxY = MinY + (f32)bitmap->height;

  i32 sourceOffsetX = 0;
  if (MinX < 0) {
    sourceOffsetX = -MinX;
    MinX = 0;
  }

  i32 sourceOffsetY = 0;
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

  u32 *sourceRow = bitmap->pixels + bitmap->width * (bitmap->height - 1);
  sourceRow += -sourceOffsetY * bitmap->width + sourceOffsetX;
  u8 *destRow = (u8 *)buffer->memory + MinY * buffer->pitch + MinX * buffer->bytesPerPixel;
  for (int y = MinY; y < MaxY; ++y) {
    u32 *dest = (u32 *)destRow;
    u32 *source = sourceRow;
    for (int x = MinX; x < MaxX; ++x) {

      u32 S = *source;
      u32 D = *dest;

      f32 alpha = (f32)((S >> 24) & 0XFF) / 255;
      f32 SR = (f32)((S >> 16) & 0XFF);
      f32 SG = (f32)((S >> 8) & 0XFF);
      f32 SB = (f32)((S >> 0) & 0XFF);

      f32 DR = (f32)((D >> 16) & 0XFF);
      f32 DG = (f32)((D >> 8) & 0XFF);
      f32 DB = (f32)((D >> 0) & 0XFF);

#define MX(C) (D##C - alpha * (D##C - S##C))
      f32 R = MX(R);
      f32 G = MX(G);
      f32 B = MX(B);

      *dest = (((u32)(R + 0.5f) << 16) |
               ((u32)(G + 0.5f) << 8) |
               ((u32)(B + 0.5f) << 0));

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
  u16 fileType;
  u32 fileSize;
  u16 reserved1;
  u16 reserved2;
  u32 bitmapOffset;
  u32 size;
  i32 width;
  i32 height;
  u16 planes;
  u16 bitsPerPixel;
  u32 compression;
  u32 sizeOfBitmap;
  i32 horzResolution;
  i32 vertResolution;
  u32 colorsUsed;
  u32 colorsImportant;

  u32 redMask;
  u32 greenMask;
  u32 blueMask;
};
#pragma pack(pop)

static loaded_bitmap DEBUGLoadBMP(thread_context *thread, debug_platform_read_entire_file *readEntireFile, const char *fileName) {
  loaded_bitmap result = {};

  debug_read_file_result readFileResult = readEntireFile(thread, fileName);
  if (readFileResult.fileSize != 0) {
    bitmap_header *header = (bitmap_header *)readFileResult.contents;
    u32 *pixels = (u32 *)((u8 *)header + header->bitmapOffset);

    result.pixels = pixels;
    result.width = header->width;
    result.height = header->height;

    u32 redMask = header->redMask;
    u32 greenMask = header->greenMask;
    u32 blueMask = header->blueMask;
    u32 alphaMask = ~(redMask | greenMask | blueMask);

    bit_scan_result redScan = findLeastSignificantSetBit(redMask);
    bit_scan_result greenScan = findLeastSignificantSetBit(greenMask);
    bit_scan_result blueScan = findLeastSignificantSetBit(blueMask);
    bit_scan_result alphaScan = findLeastSignificantSetBit(alphaMask);

    assert(redScan.found);
    assert(greenScan.found);
    assert(blueScan.found);
    assert(alphaScan.found);

    i32 redShit = 16 - (i32)redScan.index;
    i32 greenShit = 8 - (i32)greenScan.index;
    i32 blueShit = 0 - (i32)blueScan.index;
    i32 alphaShit = 24 - (i32)alphaScan.index;

    u32 *sourceDest = pixels;
    for (int y = 0; y < result.height; ++y) {
      for (int x = 0; x < result.width; ++x) {
        // RR GG BB AA -> AA RR GG BB
        u32 C = *sourceDest;
        *sourceDest++ = _rotl(C & redMask, redShit) |
                        _rotl(C & greenMask, greenShit) |
                        _rotl(C & blueMask, blueShit) |
                        _rotl(C & alphaMask, alphaShit);
      }
    }
  }

  return result;
}

inline entity *getEntity(game_state *gameState, u32 index) {
  entity *entity = nullptr;

  if (index > 0 && index < arr_length(gameState->entitys)) {
    entity = &gameState->entitys[index];
  }

  return entity;
}

inline static void initializePlayer(game_state *gameState, u32 entityIndex) {
  entity *entity = getEntity(gameState, entityIndex);

  entity->exists = true;
  entity->P.absTileX = 1;
  entity->P.absTileY = 3;
  entity->P.offset.X = 5.0f;
  entity->P.offset.Y = 5.0f;
  entity->height = 1.4f;
  entity->width = 0.75f * entity->height;

  if (!getEntity(gameState, gameState->cameraFollowingEntityIndex)) {
    gameState->cameraFollowingEntityIndex = entityIndex;
  }
}

inline static u32 addEntity(game_state *gameState) {
  u32 entityIndex = gameState->entityCount++;

  assert(gameState->entityCount < arr_length(gameState->entitys));
  entity *entity = &gameState->entitys[entityIndex];
  *entity = {};

  return entityIndex;
}

inline static void movePlayer(game_state *gameState, entity *entity, f32 dt, v2 ddP) {
  tile_map *tileMap = gameState->world->tileMap;

  if (ddP.X != 0.0f && ddP.Y != 0.0f) {
    ddP *= 0.707106781187f;
  }

  // m/s^2
  f32 playerSpeed = 50.0f;
  ddP *= playerSpeed;

  ddP += -8.0f * entity->dP;

  tile_map_position oldPlayerP = entity->P;
  tile_map_position newPlayerP = oldPlayerP;

  // 位置 p = 0.5 * a * t^2 + v * t + p
  v2 playerDelta = 0.5f * ddP * square(dt) + entity->dP * dt;
  newPlayerP.offset += playerDelta;
  // 速度
  entity->dP = ddP * dt + entity->dP;
  newPlayerP = recanonicalizePosition(tileMap, newPlayerP);

#if 1
  tile_map_position playerLeft = newPlayerP;
  playerLeft.offset.X -= 0.5f * entity->width;
  playerLeft = recanonicalizePosition(tileMap, playerLeft);

  tile_map_position playerRight = newPlayerP;
  playerRight.offset.X += 0.5f * entity->width;
  playerRight = recanonicalizePosition(tileMap, playerRight);

  bool collided = false;
  tile_map_position colP = {};
  if (!isTileMapPointEmpty(tileMap, newPlayerP)) {
    colP = newPlayerP;
    collided = true;
  }
  if (!isTileMapPointEmpty(tileMap, playerLeft)) {
    colP = playerLeft;
    collided = true;
  }
  if (!isTileMapPointEmpty(tileMap, playerRight)) {
    colP = playerRight;
    collided = true;
  }

  if (collided) {
    v2 r = {};
    if (colP.absTileX < entity->P.absTileX) {
      r = v2{{1, 0}};
    }
    if (colP.absTileX > entity->P.absTileX) {
      r = v2{{-1, 0}};
    }
    if (colP.absTileY < entity->P.absTileY) {
      r = v2{{0, 1}};
    }
    if (colP.absTileY > entity->P.absTileY) {
      r = v2{{0, -1}};
    }

    entity->dP = entity->dP - 2 * inner(entity->dP, r) * r;
  } else {
    entity->P = newPlayerP;
  }
#else
  u32 MinTileX = 0;
  u32 MinTileY = 0;
  u32 OnePastMaxTileX = 0;
  u32 OnePastMaxTileY = 0;
  u32 AbsTileZ = entity->P.absTileZ;
  tile_map_position BestPlayerP = entity->P;
  f32 BestDistanceSq = LengthSq(PlayerDelta);
  for (u32 AbsTileY = MinTileY;
       AbsTileY != OnePastMaxTileY;
       ++AbsTileY) {
    for (u32 AbsTileX = MinTileX;
         AbsTileX != OnePastMaxTileX;
         ++AbsTileX) {
      tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
      u32 TileValue = GetTileValue(TileMap, TestTileP);
      if (IsTileValueEmpty(TileValue)) {
        v2 MinCorner = -0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
        v2 MaxCorner = 0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};

        tile_map_difference RelNewPlayerP = Subtract(TileMap, &TestTileP, &NewPlayerP);
        v2 TestP = ClosestPointInRectangle(MinCorner, MaxCorner, RelNewPlayerP);
        TestDistanceSq = ;
        if (BestDistanceSq > TestDistanceSq) {
          BestPlayerP = ;
          BestDistanceSq = ;
        }
      }
    }
  }
#endif

  if (!areOnSameTile(&oldPlayerP, &entity->P)) {
    u32 tileValue = getTileValue(tileMap, entity->P);
    if (tileValue == 3) {
      ++entity->P.absTileZ;
    }
    if (tileValue == 4) {
      --entity->P.absTileZ;
    }
  }

  // 控制朝向
  if (entity->dP.X == 0.0f && entity->dP.Y == 0.0f) {
    // NOTE: Leave FacingDirection whatever it was
  } else if (fabs(entity->dP.X) > fabs(entity->dP.Y)) {
    if (entity->dP.X > 0) {
      entity->facingDirection = 0;
    } else {
      entity->facingDirection = 2;
    }
  } else {
    if (entity->dP.Y > 0) {
      entity->facingDirection = 1;
    } else {
      entity->facingDirection = 3;
    }
  }
}

extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
  static bool isWrite;

  assert(&gameInput->controller->terminator - gameInput->controller->Button == arr_length(gameInput->controller->Button));
  assert(sizeof(game_state) <= memory->permanentStorageSize);

  f32 playerHeight = 1.4f;
  f32 playerWidth = 0.75 * playerHeight;

  game_state *gameState = (game_state *)memory->permanentStorage;
  if (!memory->isInitialized) {
    addEntity(gameState);

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

    initializerArena(&gameState->worldArena,
                     memory->permanentStorageSize - sizeof(game_state),
                     (u8 *)memory->permanentStorage + sizeof(game_state));

    gameState->world = pushStruct(&gameState->worldArena, world);
    world *world = gameState->world;
    world->tileMap = pushStruct(&gameState->worldArena, tile_map);

    tile_map *tileMap = world->tileMap;

    tileMap->chunkShift = 4;
    tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
    tileMap->chunkDim = (1 << tileMap->chunkShift);

    tileMap->tileSideInMeters = 1.4f;
    tileMap->tileSideInPixels = 60;
    tileMap->metersToPixels = (f32)tileMap->tileSideInPixels / (f32)tileMap->tileSideInMeters;

    tileMap->tileChunkCountX = 128;
    tileMap->tileChunkCountY = 128;
    tileMap->tileChunkCountZ = 2;
    tileMap->tileChunks = pushArray(&gameState->worldArena,
                                    tileMap->tileChunkCountX * tileMap->tileChunkCountY * tileMap->tileChunkCountZ,
                                    tile_chunk);

    u32 tilesPerWidth = 17;
    u32 tilesPerHeight = 9;
    u32 screenX = 0;
    u32 screenY = 0;
    u32 absTileZ = 0;

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

      for (u32 tileY = 0; tileY < tilesPerHeight; ++tileY) {
        for (u32 tileX = 0; tileX < tilesPerWidth; ++tileX) {
          u32 absTileX = (screenX * tilesPerWidth) + tileX;
          u32 absTileY = (screenY * tilesPerHeight) + tileY;

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

  for (u32 controllerIndex = 0; controllerIndex < game_input::controllerCount; ++controllerIndex) {
    game_controller_input *controller = &gameInput->controller[controllerIndex];
    entity *controllerEntity = getEntity(gameState, gameState->playerIndexForController[controllerIndex]);

    if (controllerEntity) {
      v2 ddP = {};

      if (controller->isAnalog) {
        ddP = v2{{controller->stickAverageX, controller->stickAverageY}};
      } else {
        if (controller->moveUp.endDown) {
          ddP.Y = 1.0f;
        }
        if (controller->moveDown.endDown) {
          ddP.Y = -1.0f;
        }
        if (controller->moveLeft.endDown) {
          ddP.X = -1.0f;
        }
        if (controller->moveRight.endDown) {
          ddP.X = 1.0f;
        }
      }

      movePlayer(gameState, controllerEntity, gameInput->dtForFrame, ddP);
    } else {
      if (controller->start.endDown) {
        u32 entityIndex = addEntity(gameState);
        initializePlayer(gameState, entityIndex);
        gameState->playerIndexForController[controllerIndex] = entityIndex;
      }
    }
  }

  entity *cameraFollowingEntity = getEntity(gameState, gameState->cameraFollowingEntityIndex);
  if (cameraFollowingEntity) {
    gameState->CameraP.absTileZ = cameraFollowingEntity->P.absTileZ;

    auto diff = subtract(tileMap, &cameraFollowingEntity->P, &gameState->CameraP);

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

  drawBitmap(buffer, &gameState->backdrop, 0, 0, 0, 0);

  f32 screenCenterX = 0.5f * (f32)buffer->width;
  f32 screenCenterY = 0.5f * (f32)buffer->height;

  for (i32 relRow = -10; relRow < 10; ++relRow) {
    for (i32 relColumn = -20; relColumn < 20; ++relColumn) {
      u32 column = gameState->CameraP.absTileX + relColumn;
      u32 row = gameState->CameraP.absTileY + relRow;
      u32 tileID = getTileValue(tileMap, column, row, gameState->CameraP.absTileZ);

      // 零表示空
      if (tileID < 2) {
        continue;
      }

      f32 gray = 0.5f;
      if (tileID == 2) {
        gray = 1.0f;
      }

      if (tileID > 2) {
        gray = 0.25f;
      }

      if (column == gameState->CameraP.absTileX && row == gameState->CameraP.absTileY) {
        gray = 0.0f;
      }

      v2 tileSid = {{(f32)(0.5 * tileMap->tileSideInPixels), (f32)(0.5 * tileMap->tileSideInPixels)}};
      v2 cen = {{screenCenterX - tileMap->metersToPixels * gameState->CameraP.offset.X + ((f32)relColumn) * tileMap->tileSideInPixels,
                 screenCenterY + tileMap->metersToPixels * gameState->CameraP.offset.Y - ((f32)relRow) * tileMap->tileSideInPixels}};
      v2 Min = cen - tileSid;
      v2 Max = cen + tileSid;

      drawRectangle(buffer, Min, Max, gray, gray, gray);
    }
  }

  entity *entity = gameState->entitys;
  for (u32 entityIndex = 0; entityIndex < gameState->entityCount; ++entityIndex, ++entity) {
    if (entity->exists) {
      auto diff = subtract(tileMap, &entity->P, &gameState->CameraP);

      f32 playerR = 1.0f;
      f32 playerG = 1.0f;
      f32 playerB = 0.0f;
      f32 playerGroundPointX = screenCenterX + diff.dXY.X * tileMap->metersToPixels;
      f32 playerGroundPointY = screenCenterY - diff.dXY.Y * tileMap->metersToPixels;

      v2 playerLeftTop = {{playerGroundPointX - (f32)(0.5 * playerWidth * tileMap->metersToPixels), playerGroundPointY - playerHeight * tileMap->metersToPixels}};
      v2 playerWidthHeight = {{playerWidth, playerHeight}};
      drawRectangle(buffer, playerLeftTop, playerLeftTop + playerWidthHeight * tileMap->metersToPixels, playerR, playerG, playerB);

      hero_bitmaps *heroBitmaps = &gameState->heroBitmaps[entity->facingDirection];
      drawBitmap(buffer, &heroBitmaps->Head, playerGroundPointX, playerGroundPointY, heroBitmaps->alignX, heroBitmaps->alignY);
      drawBitmap(buffer, &heroBitmaps->Cape, playerGroundPointX, playerGroundPointY, heroBitmaps->alignX, heroBitmaps->alignY);
      drawBitmap(buffer, &heroBitmaps->Torso, playerGroundPointX, playerGroundPointY, heroBitmaps->alignX, heroBitmaps->alignY);
    }
  }
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
  game_state *gameState = (game_state *)memory->permanentStorage;
  game_output_sound(soundOutputBuffer, gameState);
}
