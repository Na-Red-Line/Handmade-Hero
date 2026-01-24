#pragma once

#include "handmade.h"

#include <dsound.h>
#include <windows.h>
#include <Xinput.h>

constexpr int WIN_STATE_FILE_NAME_COUNT = MAX_PATH;

struct win_offscreen_buffer {
  BITMAPINFO info;
  VOID *memory;
  int width;
  int height;
  int bytesPerPixel = 4;
  int pitch;
};

struct win_window_dimension {
  int width;
  int height;
};

struct win_sound_output {
  uint32 runingSampleIndex; // 索引
  int samplesPerSecond;     // 赫兹
  int toneVolume;           // 音高
  int toneHz;               // 一秒钟震动次数
  int wavePeroid;           // 每秒采样数
  int bytesPerSample;       // 双声道，左右各16比特，2字节
  int DSoundBufferSize;     // 缓冲区大小
  int safetyBytes;          // 每帧缓冲区安全值
};

struct win_debug_time_marker {
  DWORD outputPlayCursor;
  DWORD outputWriteCursor;
  DWORD outputLocation;
  DWORD outputByteCount;
  DWORD expectedFlipPlayCursor;

  DWORD flipPlayCursor;
  DWORD flipWriteCursor;
};

struct win_game_code {
  HMODULE gameCodeDLL;
  FILETIME DLLLastWriteTime;

  // 这两个函数都可能为空，调用前必须进行检查
  game_update_and_render *updateAndRender;
  game_get_sound_samples *getSoundSamples;

  bool isValid;
};

struct win_replay_buffer {
  HANDLE fileHandle;
  HANDLE memoryMap;
  char filename[WIN_STATE_FILE_NAME_COUNT];
  void *memoryBlock;
};

// 游戏状态
struct win_state {
  // 游戏内存总大小
  uint64 totalSize;
  // 内存基址
  void *gameMemoryBlock;
  win_replay_buffer replayBuffers[4];

  HANDLE recordingHandle;
  int inputRecordingIndex;

  HANDLE playbackHandle;
  int inputPlayingIndex;

  char EXEFileName[WIN_STATE_FILE_NAME_COUNT];
  size_t onePastLastSlashSize;
};
