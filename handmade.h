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
constexpr int arr_length(T (&)[N]) { return N; }

constexpr uint64 KiloBytes(uint64 x) { return x << 10; }
constexpr uint64 MegaBytes(uint64 x) { return KiloBytes(x) << 10; }
constexpr uint64 GigaBytes(uint64 x) { return MegaBytes(x) << 10; }
constexpr uint64 TeraBytes(uint64 x) { return GigaBytes(x) << 10; }

constexpr uint32 saveCastUint64(uint64 value) {
  assert(value <= 0xffffffff);
  return (uint32)(value);
}

struct thread_context {
  int placeholder;
};

#if HANDMADE_INTERNAL
// IMPORT 测试使用 平台无关文件IO

struct debug_read_file_result {
  uint32 fileSize;
  void *contents;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *thread, void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *thread, const char *filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(thread_context *thread, const char *filename, uint32 memorySize, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

// 更新屏幕渲染
struct game_offscreen_buffer {
  void *memory;
  int width;
  int height;
  int pitch;
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
  float stickAverageX;
  float stickAverageY;

  union {
    game_button_state Button[12];
    struct {
      // 移动
      game_button_state moveUp;
      game_button_state moveDown;
      game_button_state moveLeft;
      game_button_state moveRight;

      // action
      game_button_state actionUp;
      game_button_state actionDown;
      game_button_state actionLeft;
      game_button_state actionRight;

      game_button_state leftShoulder;
      game_button_state rightShoulder;

      game_button_state start;
      game_button_state back;

      // 分界线，用于边界断言
      game_button_state terminator;
    };
  };

  bool isAnalog; // 是否是摇杆
  bool isConnected;
};

struct game_input {
  // 鼠标控制器
  game_button_state mouseButtons[5];
  int mouseX;
  int mouseY;
  int mouseZ;

  float dtForFrame;

  // 五个控制器，第一个是键盘
  game_controller_input controller[5];
};

struct canonical_position {
  int32 tileMapX;
  int32 tileMapY;

  int32 tileX;
  int32 tileY;

  float tileRelX;
  float tileRelY;
};

struct raw_position {
  int32 tileMapX;
  int32 tileMapY;

  float X;
  float Y;
};

struct tile_map {
  uint32 *tiles;
};

struct world {
  int32 countX;
  int32 countY;

  float upperLeftX;
  float upperLeftY;
  float tileWidth;
  float tileHeight;

  int32 tileMapCountX;
  int32 tileMapCountY;

  tile_map *tileMaps;
};

struct game_state {
  int32 playerTileMapX;
  int32 playerTileMapY;

  float playerX;
  float playerY;
};

struct game_memory {
  bool isInitialized;

  // NOTE 永久游戏内存，暂为 game_state
  uint64 permanentStorageSize;
  void *permanentStorage;

  // NOTE 动态游戏内存，暂为 game_state
  uint64 transientStorageSize;
  void *transientStorage;

#if HANDMADE_INTERNAL
  debug_platform_free_file_memory *debugPlatformFreeFileMemory;
  debug_platform_read_entire_file *debugPlatformReadEntireFile;
  debug_platform_write_entire_file *debugPlatformWriteEntireFile;
#endif
};

// 动态链接加载方法
#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *thread, game_memory *memory, game_input *gameInput, game_offscreen_buffer *buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *thread, game_memory *memory, game_sound_output_buffer soundOutputBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
