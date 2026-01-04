#include "inc.h"

void game_output_sound(game_sound_output_buffer sound_output_buffer) {
  // 余弦x轴坐标
  static float tSine = 0;

  int16 *sampleOut = (int16 *)sound_output_buffer.buffer;
  for (int sampleIndex = 0; sampleIndex < sound_output_buffer.bufferSize; ++sampleIndex) {
    float sineValue = sinf(tSine); // 余弦y轴坐标
    int sampleValue = sineValue * sound_output_buffer.toneVolume;
    // 写入左右双声道
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
    // 适应频率改变
    tSine += (PI * 2.0f) / (float)sound_output_buffer.wavePeroid;
  }
}

void renderWeirGradient(game_offscreen_buffer buffer) {
  auto [memory, width, height, bytesPerPixel, xOffset, yOffset] = buffer;

  uint8 *row = (uint8 *)memory;
  int pitch = width * bytesPerPixel;

  for (int y = 0; y < height; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = 0; x < width; ++x) {
      // memory order: RR GG BB xx
      // loaded in:    xx BB GG RR
      // window:       xx RR GG BB
      // memory order: BB GG RR xx
      uint8 blue = x + xOffset;
      uint8 green = y + yOffset;
      *pixel++ = ((green << 8) | blue);
    }
    row += pitch;
  }
}

void gameUpdateAndRender(game_offscreen_buffer offscreen_buffer, game_sound_output_buffer sound_output_buffer) {
  game_output_sound(sound_output_buffer);
  renderWeirGradient(offscreen_buffer);
}
