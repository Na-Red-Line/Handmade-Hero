#include "inc.h"

#include <dsound.h>
#include <windows.h>
#include <Xinput.h>

struct win64_offscreen_buffer {
  BITMAPINFO info;
  VOID *memory;
  int width;
  int height;
  int bytesPerPixel = 4;
  int pitch;
};

struct win64_window_dimension {
  int width;
  int height;
};

struct win64_sound_output {
  uint32 runingSampleIndex; // 索引
  int samplesPerSecond;     // 赫兹
  int toneVolume;           // 音高
  int toneHz;               // 一秒钟震动次数
  int wavePeroid;           // 每秒采样数
  int bytesPerSample;       // 双声道，左右各16比特，2字节
  int DSoundBufferSize;     // 缓冲区大小
  int latencySampleCount;   // 声音延迟
};
