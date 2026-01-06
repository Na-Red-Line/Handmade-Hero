#pragma once

#include <assert.h>
#include <math.h>
#include <stdint.h>

constexpr float PI = 3.14159265359f;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

template <typename T, int N>
consteval int arr_length(T (&)[N]) { return N; }

consteval uint64 KiloBytes(uint64 x) { return x << 10; }
consteval uint64 MegaBytes(uint64 x) { return KiloBytes(x) << 10; }
consteval uint64 GigaBytes(uint64 x) { return MegaBytes(x) << 10; }
consteval uint64 TeraBytes(uint64 x) { return GigaBytes(x) << 10; }

constexpr uint32 saveCastUint64(uint64 value) {
  assert(value <= 0xffffffff);
  return (uint32)(value);
}

#ifdef HANDMADE_INTERNAL
// IMPORT 测试使用

struct debug_read_file_result {
  uint32 fileSize;
  void *memory;
};
debug_read_file_result DEBUGPlatformReadFile(char *filename);
void DEBUGPlatformFreeMemory(void *memory);
bool DEBUGPlatformWriteEntireFile(char *filename, uint32 memorySize, void *memory);
#endif

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

  bool isAnalog; // 是否是摇杆
};

struct game_input {
  game_controller_input controller[4];
};

struct game_state {
  int blueOffset;
  int greenOffset;
  int toneHz;
};

struct game_memory {
  bool isInitialized;
  uint64 permanentStorageSize;
  void *permanentStorage;
};

void gameUpdateAndRender(game_memory *memory, game_input *gameInput, game_offscreen_buffer offscreenBuffer, game_sound_output_buffer soundOutputBuffer);
