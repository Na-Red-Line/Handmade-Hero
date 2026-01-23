#include "handmade.h"

static void game_output_sound(game_sound_output_buffer soundOutputBuffer, game_state *gameState) {
  int16 *sampleOut = (int16 *)soundOutputBuffer.buffer;
  float wavePeroid = (float)soundOutputBuffer.samplesPerSecond / (float)gameState->toneHz;

  for (int sampleIndex = 0; sampleIndex < soundOutputBuffer.bufferSize; ++sampleIndex) {
    float sineValue = sinf(gameState->tSine); // 余弦y轴坐标
    int sampleValue = sineValue * soundOutputBuffer.toneVolume;
    // 写入左右双声道
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
    // 适应频率改变
    gameState->tSine += (PI * 2.0f) / wavePeroid;
    // 防止精度丢失
    if (gameState->tSine > PI * 2.0f) gameState->tSine -= PI * 2.0f;
  }
}

static void renderPlayer(game_offscreen_buffer buffer, int playX, int playY) {
  uint8 *endOfBuffer = (uint8 *)buffer.memory + buffer.height * buffer.pitch;

  uint32 color = 0xffffffff;
  int size = 10;

  for (int x = playX; x < playX + size; ++x) {
    uint8 *pixel = (uint8 *)buffer.memory + playY * buffer.pitch + x * buffer.bytesPerPixel;
    for (int y = playY; y < playY + size; ++y) {
      if (pixel >= buffer.memory && (pixel + 4) <= endOfBuffer) {
        *(uint32 *)pixel = color;
      }
      pixel += buffer.pitch;
    }
  }
};

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

extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
  static bool isWrite;

  assert(&gameInput->controller->terminator - gameInput->controller->Button == arr_length(gameInput->controller->Button));
  assert(sizeof(game_state) <= memory->permanentStorageSize);

  game_state *gameState = (game_state *)memory->permanentStorage;

  if (!memory->isInitialized) {
#if HANDMADE_INTERNAL
    if (!isWrite) {
      char filename[] = __FILE__;
      auto result = memory->debugPlatformReadEntireFile(thread, filename);
      if (result.contents) {
        memory->debugPlatformWriteEntireFile(thread, "test.out", result.fileSize, result.contents);
        memory->debugPlatformFreeFileMemory(thread, result.contents);
      }
      isWrite = true;
    }
#endif

    gameState->toneHz = 256;
    gameState->tSine = .0f;

    gameState->playerX = 100;
    gameState->playerY = 100;

    memory->isInitialized = true;
  }

  for (auto &input : gameInput->controller) {
    if (input.isAnalog) {
      gameState->blueOffset += (int)(4.f * input.stickAverageX);
      gameState->toneHz = 256 + (int)(128.f * input.stickAverageY);
    } else {
      if (input.moveLeft.endDown) {
        gameState->blueOffset += -1;
      } else if (input.moveRight.endDown) {
        gameState->blueOffset += 1;
      }
    }

    if (input.actionDown.endDown) {
      gameState->greenOffset += 1;
    }

    gameState->playerX += (int)(4.0f * input.stickAverageX);
    gameState->playerY -= (int)(4.0f * input.stickAverageY);
    if (gameState->tJump > 0) {
      gameState->playerY += int(1.5f * sinf(0.5f * PI * gameState->tJump));
    }
    if (input.actionLeft.endDown) {
      gameState->tJump = 4.0f;
    }
    gameState->tJump -= 4.0 / (float)(60 * 2 * arr_length(gameInput->controller));
  }

  renderWeirGradient(offscreenBuffer, gameState->blueOffset, gameState->greenOffset);
  renderPlayer(offscreenBuffer, gameState->playerX, gameState->playerY);
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
  game_state *gameState = (game_state *)memory->permanentStorage;
  game_output_sound(soundOutputBuffer, gameState);
}
