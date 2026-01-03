#ifndef INC_H
#define INC_H

#include <assert.h>
#include <math.h>
#include <stdint.h>

#define PI 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

// 更新屏幕渲染
struct game_offscreen_buffer {
  void *memory;
  int width;
  int height;
  int bytesPerPixel;
  int xOffset;
  int yOffset;
};

struct game_sound_output_buffer {
  void *buffer;   // 缓冲区
  int bufferSize; // 缓冲区大小
  int toneVolume; // 音高
  int wavePeroid; // 每秒采样数
};

void gameUpdateAndRender(game_offscreen_buffer offscreen_buffer, game_sound_output_buffer sound_output_buffer);

#endif
