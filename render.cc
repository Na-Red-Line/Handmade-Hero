#include "inc.h"

void game_output_sound(game_sound_output_buffer soundOutputBuffer, int toneHz) {
  // 余弦x轴坐标
  static float tSine = 0;

  int16 *sampleOut = (int16 *)soundOutputBuffer.buffer;
  float wavePeroid = (float)soundOutputBuffer.samplesPerSecond / (float)toneHz;

  for (int sampleIndex = 0; sampleIndex < soundOutputBuffer.bufferSize; ++sampleIndex) {
    float sineValue = sinf(tSine); // 余弦y轴坐标
    int sampleValue = sineValue * soundOutputBuffer.toneVolume;
    // 写入左右双声道
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
    // 适应频率改变
    tSine += (PI * 2.0f) / wavePeroid;
    // 防止精度丢失
    if (tSine > PI * 2.0f) tSine -= PI * 2.0f;
  }
}

void renderWeirGradient(game_offscreen_buffer offscreenBuffer, int blueOffset, int greenOffset) {
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

void gameUpdateAndRender(game_memory *memory, game_input *gameInput, game_offscreen_buffer offscreenBuffer) {
  assert(&gameInput->controller->terminator - gameInput->controller->Button == arr_length(gameInput->controller->Button));
  assert(sizeof(game_state) <= memory->permanentStorageSize);

  game_state *gameState = (game_state *)memory->permanentStorage;

  if (!memory->isInitialized) {
#if 0
		char *filename = (char *)__FILE__;
    auto result = DEBUGPlatformReadFile(filename);
    if (result.memory) {
      DEBUGPlatformWriteEntireFile((char *)"test.out", result.fileSize, result.memory);
      DEBUGPlatformFreeMemory(result.memory);
    }
#endif

    gameState->toneHz = 256;
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
  }

  renderWeirGradient(offscreenBuffer, gameState->blueOffset, gameState->greenOffset);
}

void gameGetSoundSamples(game_memory *memory, game_sound_output_buffer soundOutputBuffer) {
  game_state *gameState = (game_state *)memory->permanentStorage;
  game_output_sound(soundOutputBuffer, gameState->toneHz);
}
