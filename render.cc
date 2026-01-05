#include "inc.h"

void game_output_sound(game_sound_output_buffer soundOutputBuffer, int toneHz) {
  // 余弦x轴坐标
  static float tSine = 0;

  int16 *sampleOut = (int16 *)soundOutputBuffer.buffer;
  float wavePeroid = (float)soundOutputBuffer.samplesPerSecond / toneHz;

  for (int sampleIndex = 0; sampleIndex < soundOutputBuffer.bufferSize; ++sampleIndex) {
    float sineValue = sinf(tSine); // 余弦y轴坐标
    int sampleValue = sineValue * soundOutputBuffer.toneVolume;
    // 写入左右双声道
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
    // 适应频率改变
    tSine += (PI * 2.0f) / wavePeroid;
  }
}

void renderWeirGradient(game_offscreen_buffer offscreenBuffer, int blueOffset, int greenOffset) {
  auto [memory, width, height, bytesPerPixel] = offscreenBuffer;

  uint8 *row = (uint8 *)memory;
  int pitch = width * bytesPerPixel;

  for (int y = 0; y < height; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = 0; x < width; ++x) {
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

void gameUpdateAndRender(game_memory *memory, game_input *gameInput, game_offscreen_buffer offscreenBuffer, game_sound_output_buffer soundOutputBuffer) {
  assert(sizeof(game_state) <= memory->permanentStorageSize);
  game_state *gameState = (game_state *)memory->permanentStorage;

  if (!memory->isInitialized) {
    gameState->toneHz = 256;
    memory->isInitialized = true;
  }

  game_controller_input *input0 = gameInput->controller;
  if (input0) {
    if (input0->isAnalog) {
      gameState->toneHz = 256 + (int)(128.f * input0->EndX);
      gameState->blueOffset += (int)(4.f * input0->EndY);
    } else {
      // TODO 其他操作
    }

    if (input0->down.endDown) {
      gameState->greenOffset += 1;
    }
  }

  game_output_sound(soundOutputBuffer, gameState->toneHz);
  renderWeirGradient(offscreenBuffer, gameState->blueOffset, gameState->greenOffset);
}
