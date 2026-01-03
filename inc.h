#ifndef INC_H
#define INC_H

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

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
};

void renderWeirGradient(game_offscreen_buffer buffer, int xOffset, int yOffset);

#endif
