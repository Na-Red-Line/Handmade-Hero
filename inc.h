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

template <typename T, int N>
consteval int arr_length(T (&arr)[N]) { return N; }

// 更新屏幕渲染
struct game_offscreen_buffer {
  void *memory;
  int width;
  int height;
  int bytesPerPixel;
};

struct game_sound_output_buffer {
  void *buffer;         // 缓冲区
  int bufferSize;       // 缓冲区大小
  int samplesPerSecond; // 赫兹
  int toneVolume;       // 音高
};

struct game_button_state {
  int halfTransitionCount;
  int32 endDown;
};

struct game_controller_input {
  bool isAnalog;

  float startX;
  float startY;

  float minX;
  float minY;

  float EndX;
  float EndY;

  union {
    game_button_state Button[6];
    struct {
      game_button_state up;
      game_button_state down;
      game_button_state left;
      game_button_state right;
      game_button_state leftShoulder;
      game_button_state rightShoulder;
    };
  };
};

struct game_input {
  game_controller_input controller[4];
};

void gameUpdateAndRender(game_input *gameInput, game_offscreen_buffer offscreenBuffer, game_sound_output_buffer soundOutputBuffer);

#endif
