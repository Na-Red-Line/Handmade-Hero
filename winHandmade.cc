#ifndef UNICODE
#define UNICODE
#endif

#include <stdio.h>
#include "windows.h"

static bool globalRuning;
static bool globalPause;
static bool globalMouseOn;
static win_offscreen_buffer globalBackBuffer;
static LPDIRECTSOUNDBUFFER globalDSoundBuffer;
// 每秒计数器递增的次数
static LONGLONG globalPerfCountFrequency;

// 优雅降级
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(xInputGetState) { return 0; }
static x_input_get_state *XInputGetState_ = xInputGetState;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(xInputSetState) { return 0; }
static x_input_set_state *XInputSetState_ = xInputSetState;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
DIRECT_SOUND_CREATE(directSoundCreate) { return 0; }
static direct_sound_create *DirectSoundCreate_ = directSoundCreate;
#define DirectSoundCreate DirectSoundCreate_

#if HANDMADE_INTERNAL
DEBUG_PLATFORM_FREE_FILE_MEMORY(debugPlatformFreeFileMemory) {
  if (memory) VirtualFree(memory, 0, 0);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(debugPlatformReadEntireFile) {
  debug_read_file_result result = {0, nullptr};
  HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (fileHandle == INVALID_HANDLE_VALUE) return result;

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(fileHandle, &fileSize)) return result;

  result.contents = VirtualAlloc(0, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!result.contents) {
    // TODO 读取文件内存分配失败
  }

  DWORD bytesRead;
  result.fileSize = saveCastUint64(fileSize.QuadPart);
  if (ReadFile(fileHandle, result.contents, result.fileSize, &bytesRead, 0) && result.fileSize == bytesRead) {
    // NOTE 读取成功
  } else {
    debugPlatformFreeFileMemory(thread, result.contents);
    result.contents = nullptr;
  }

  CloseHandle(fileHandle);
  return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debugPlatformWriteEntireFile) {
  bool result = false;

  HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (fileHandle == INVALID_HANDLE_VALUE) return result;

  DWORD bytesRead;
  if (WriteFile(fileHandle, memory, memorySize, &bytesRead, 0)) {
    // NOTE 写入成功
    result = bytesRead == memorySize;
  } else {
    // TODO logging
  }

  CloseHandle(fileHandle);
  return result;
}
#endif

static void winGetEXEFileName(win_state *state) {
  // 获取主进程当前路径
  DWORD sizeOfFilename = GetModuleFileNameA(0, state->EXEFileName, sizeof(state->EXEFileName));
  // 找到最后一个文件分隔符并
  for (size_t scan = sizeOfFilename - 1; scan > 0; --scan)
    if (state->EXEFileName[scan] == '\\') {
      state->onePastLastSlashSize = scan + 1;
      break;
    }
}

static int stringLength(const char *string) {
  int count = 0;
  while (*string++) {
    count++;
  }
  return count;
}

static void winBuildEXEPathFileName(win_state *state, const char *fileName, char *dest) {
  char *EXEPath = state->EXEFileName;
  int EXEPathCount = state->onePastLastSlashSize;

  while (EXEPathCount-- > 0) {
    *dest++ = *EXEPath++;
  }

  int fileNameCount = stringLength(fileName);
  while (fileNameCount-- > 0) {
    *dest++ = *fileName++;
  }
  *dest = '\0';
}

// 动态读取加载游戏相关代码
// 获取DLL文件最后更新时间，如果发现更新则卸载重读DLL
// 可以做到一帧延迟更新
static FILETIME winGetLastWriteTime(char *filename) {
  FILETIME lastWriteTime = {};

  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) {
    lastWriteTime = data.ftLastWriteTime;
  }

  return lastWriteTime;
}

static win_game_code winLoadGameCode(char *sourceDLLName, char *tempDLLName) {
  win_game_code result = {};

  result.DLLLastWriteTime = winGetLastWriteTime(sourceDLLName);
  CopyFileA(sourceDLLName, tempDLLName, FALSE);
  result.gameCodeDLL = LoadLibraryA(tempDLLName);
  if (result.gameCodeDLL) {
    result.updateAndRender = (game_update_and_render *)GetProcAddress(result.gameCodeDLL, "gameUpdateAndRender");
    result.getSoundSamples = (game_get_sound_samples *)GetProcAddress(result.gameCodeDLL, "gameGetSoundSamples");
    result.isValid = result.updateAndRender && result.getSoundSamples;
  }

  if (!result.isValid) {
    result.updateAndRender = nullptr;
    result.getSoundSamples = nullptr;
  }

  return result;
}

// 动态读取卸载游戏相关代码
static void winUnloadGameCode(win_game_code *gameCode) {
  if (gameCode->gameCodeDLL) {
    FreeLibrary(gameCode->gameCodeDLL);
    gameCode->gameCodeDLL = 0;
  }

  gameCode->isValid = false;
  gameCode->updateAndRender = nullptr;
  gameCode->getSoundSamples = nullptr;
}

// 初始化音频API
static void winLoadInitDSound(HWND window, int32 samplesPerSecond, int32 bufferSize) {
  // Load the dsound library
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
  if (DSoundLibrary)
    DirectSoundCreate_ = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

  LPDIRECTSOUND directSound;
  // 使用默认音频设备
  if (FAILED(DirectSoundCreate(0, &directSound, 0))) return;

  // 左右双声道，各16bit
  WAVEFORMATEX waveFormat = {
      WAVE_FORMAT_PCM,         // 未压缩的 PCM 音频
      2,                       // 立体声（双声道）
      (DWORD)samplesPerSecond, // 采样率（如 48000 Hz）
      0,
      0,
      16, // 每个采样点 16 位深度
      0,
  };
  // 每个采样帧的字节数 = 通道数 × 位深度 / 8
  waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
  // 每秒的字节数 = 采样率 × 块对齐
  waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

  // DSSCL_PRIORITY -> 可以设置主缓冲区格式
  if (FAILED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY))) return;

  // DSBCAPS_PRIMARYBUFFER -> 该缓冲区是主缓冲区（用于设置声卡格式）
  DSBUFFERDESC bufferDescription = {sizeof(bufferDescription), DSBCAPS_PRIMARYBUFFER};
  LPDIRECTSOUNDBUFFER primaryBuffer;
  if (FAILED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0))) return;

  if (FAILED(primaryBuffer->SetFormat(&waveFormat))) return;

  // 创建一个辅助缓冲区（真正的缓冲区）
  DSBUFFERDESC secondaryBufferDescription = {
      sizeof(secondaryBufferDescription),
      0,
      (DWORD)bufferSize,
      0,
      &waveFormat,
  };

  HRESULT result = (directSound->CreateSoundBuffer(&secondaryBufferDescription, &globalDSoundBuffer, 0));
  if (FAILED(result)) {
    // TODO 错误处理
  }
}

// 初始化手柄控制器API
static void winLoadXInput() {
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
  if (!XInputLibrary) XInputLibrary = LoadLibraryA("xinput1_3.dll");

  if (XInputLibrary) {
    XInputGetState_ = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState_ = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

static win_window_dimension getWindowDimension(HWND window) {
  win_window_dimension result;

  RECT clientRect;
  GetClientRect(window, &clientRect);
  result.width = clientRect.right - clientRect.left;
  result.height = clientRect.bottom - clientRect.top;

  return result;
}

static void winResizeDIBSection(win_offscreen_buffer *buffer, int width, int height) {

  // TODO bulletproof this.
  // maybe don't free first, free after, then free first if that fails.

  if (buffer->memory) {
    VirtualFree(buffer->memory, 0, MEM_RELEASE);
  }

  buffer->width = width;
  buffer->height = height;

  buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
  buffer->info.bmiHeader.biWidth = width;
  buffer->info.bmiHeader.biHeight = -height; // explain from top to bottom
  buffer->info.bmiHeader.biPlanes = 1;
  buffer->info.bmiHeader.biBitCount = 32;
  buffer->info.bmiHeader.biCompression = BI_RGB;

  int bitmapMemorySize = (width * height) * buffer->bytesPerPixel;
  buffer->pitch = buffer->width * buffer->bytesPerPixel;
  buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void winDisplayBufferInWindow(win_offscreen_buffer *buffer, HDC deviceContext, int windowWidth, int windowHeight) {
  constexpr int offsetX = 10;
  constexpr int offsetY = 10;

  PatBlt(deviceContext, 0, 0, windowWidth, offsetY, BLACKNESS);
  PatBlt(deviceContext, 0, offsetY + buffer->height, windowWidth, windowHeight, BLACKNESS);
  PatBlt(deviceContext, 0, 0, offsetX, windowHeight, BLACKNESS);
  PatBlt(deviceContext, offsetX + buffer->width, 0, windowWidth, windowHeight, BLACKNESS);

  // 原型开发不进行拉伸，保持像素 1:1
  StretchDIBits(deviceContext,
                offsetX, offsetY, buffer->width, buffer->height,
                0, 0, buffer->width, buffer->height,
                buffer->memory,
                &buffer->info,
                DIB_RGB_COLORS, SRCCOPY);
}

static void winFillSoundBuffer(win_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_sound_output_buffer *sound_output_buffer) {
  // 加锁获取可写内存指针
  VOID *region1;
  VOID *region2;
  DWORD region1Size;
  DWORD region2Size;
  if (FAILED(globalDSoundBuffer->Lock(byteToLock, bytesToWrite,
                                      &region1, &region1Size,
                                      &region2, &region2Size,
                                      0)))
    return;

  int16 *buffer = (int16 *)sound_output_buffer->buffer;
  int16 *sampleOut = (int16 *)region1;
  DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
  for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
    *sampleOut++ = *buffer++;
    *sampleOut++ = *buffer++;
    ++soundOutput->runingSampleIndex;
  }

  sampleOut = (int16 *)region2;
  DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
  for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
    *sampleOut++ = *buffer++;
    *sampleOut++ = *buffer++;
    ++soundOutput->runingSampleIndex;
  }

  globalDSoundBuffer->Unlock(region1, region1Size, region2, region2Size);
}

static void winCleanSoundBuffer(win_sound_output *soundOutput) {
  VOID *ptr1;
  VOID *ptr2;
  DWORD ptr1Size;
  DWORD ptr2Size;
  DWORD bytes = soundOutput->DSoundBufferSize;
  if (FAILED(globalDSoundBuffer->Lock(0, bytes, &ptr1, &ptr1Size, &ptr2, &ptr2Size, 0))) return;

  memset(ptr1, 0, ptr1Size);
  memset(ptr2, 0, ptr2Size);

  globalDSoundBuffer->Unlock(ptr1, ptr1Size, ptr2, ptr2Size);
}

static void initPerfCountFrequency() {
  LARGE_INTEGER lpFrequency;
  QueryPerformanceFrequency(&lpFrequency);
  globalPerfCountFrequency = lpFrequency.QuadPart;
}

static LARGE_INTEGER winGetWallClock() {
  LARGE_INTEGER result;
  QueryPerformanceCounter(&result);
  return result;
}

static float winGetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
  float secondsElapsedForWork = (float)(end.QuadPart - start.QuadPart) / (float)globalPerfCountFrequency;
  return secondsElapsedForWork;
}

// 更新手柄按键状态并计算变化数
static void winProcessXInputDigitalButton(game_button_state *newState, game_button_state *oldState, WORD wButtons, DWORD buttonBit) {
  newState->endDown = (wButtons & buttonBit) == buttonBit;
  newState->halfTransitionCount = newState->endDown != oldState->endDown;
}

// 更新键盘状态并计算变化数
static void winProcessKeyboardMessage(game_button_state *newState, bool isDown) {
  if (newState->endDown != isDown) {
    newState->endDown = isDown;
    ++newState->halfTransitionCount;
  }
}

static void winGetInputFileLocation(win_state *winState, bool inputStream, int slotIndex, char *dest) {
  char temp[64];
  sprintf(temp, "loop_edit_%d_%s.hmi", slotIndex, inputStream ? "input" : "state");
  winBuildEXEPathFileName(winState, temp, dest);
}

static win_replay_buffer *winGetReplayBuffer(win_state *state, unsigned index) {
  assert(index < arr_length(state->replayBuffers));
  win_replay_buffer *result = &state->replayBuffers[index];
  return result;
}

// 开始录制输入 写入此时的游戏内存快照
static void winBeginRecordingInput(win_state *state, int inputRecordingIndex) {
  win_replay_buffer *replayBuffer = winGetReplayBuffer(state, inputRecordingIndex);
  if (!replayBuffer->memoryBlock) return;

  state->inputRecordingIndex = inputRecordingIndex;

  char fileName[WIN_STATE_FILE_NAME_COUNT];
  winGetInputFileLocation(state, true, inputRecordingIndex, fileName);
  state->recordingHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

  CopyMemory(replayBuffer->memoryBlock, state->gameMemoryBlock, state->totalSize);
}

// 结束录制输入，清理资源
static void winEndRecordingInput(win_state *winState) {
  CloseHandle(winState->recordingHandle);
  winState->inputRecordingIndex = 0;
}

// 开始回放 游戏状态回到记录开始时
static void winBeginInputPlayBack(win_state *state, int inputPlayingIndex) {
  win_replay_buffer *replayBuffer = winGetReplayBuffer(state, inputPlayingIndex);
  if (!replayBuffer->memoryBlock) return;

  state->inputPlayingIndex = inputPlayingIndex;

  char fileName[WIN_STATE_FILE_NAME_COUNT];
  winGetInputFileLocation(state, true, inputPlayingIndex, fileName);
  state->playbackHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  CopyMemory(state->gameMemoryBlock, replayBuffer->memoryBlock, state->totalSize);
}

// 结束回放，清理资源
static void winEndInputPlayBack(win_state *winState) {
  CloseHandle(winState->playbackHandle);
  winState->inputPlayingIndex = 0;
}

// 录制输入数据
static void winRecordInput(win_state *winState, game_input *newInput) {
  DWORD bytesWritten;
  WriteFile(winState->recordingHandle, newInput, sizeof(*newInput), &bytesWritten, 0);
}

// 回放输入
static void winPlayBackInput(win_state *winState, game_input *newInput) {
  DWORD bytesRead = 0;
  if (ReadFile(winState->playbackHandle, newInput, sizeof(*newInput), &bytesRead, 0)) {
    if (bytesRead == 0) {
      // NOTE 检测到文件末尾（bytesRead == 0）重新打开文件并从头读取，自动循环回放
      int PlayingIndex = winState->inputPlayingIndex;
      winEndInputPlayBack(winState);
      winBeginInputPlayBack(winState, PlayingIndex);
      ReadFile(winState->playbackHandle, newInput, sizeof(*newInput), &bytesRead, 0);
    }
  }
}

// 处理windows消息
static void winProcessPendingMessage(win_state *state, game_controller_input *keyboardController) {
  MSG msg;
  while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {

    switch (msg.message) {
    case WM_QUIT:
      globalRuning = false;
      break;

    case WM_SYSKEYDOWN: // 系统键按下
    case WM_SYSKEYUP:   // 系统键释放
    case WM_KEYDOWN:    // 普通键按下
    case WM_KEYUP: {    // 普通键释放
      uint64 VKCode = msg.wParam;
      boolean wasDown = (msg.lParam & (1 << 30)) != 0; //  之前 按下|松开
      boolean isDown = (msg.lParam & (1 << 31)) == 0;  // 当前 按下|松开

      // 切换键盘状态才会触发相关事件
      if (wasDown != isDown) {
        if (VKCode == 'W') {
          winProcessKeyboardMessage(&keyboardController->moveUp, isDown);
        } else if (VKCode == 'A') {
          winProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
        } else if (VKCode == 'S') {
          winProcessKeyboardMessage(&keyboardController->moveDown, isDown);
        } else if (VKCode == 'D') {
          winProcessKeyboardMessage(&keyboardController->moveRight, isDown);
        } else if (VKCode == 'Q') {
          winProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
        } else if (VKCode == 'E') {
          winProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
        } else if (VKCode == VK_UP) {
          winProcessKeyboardMessage(&keyboardController->actionUp, isDown);
        } else if (VKCode == VK_LEFT) {
          winProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
        } else if (VKCode == VK_DOWN) {
          winProcessKeyboardMessage(&keyboardController->actionDown, isDown);
        } else if (VKCode == VK_RIGHT) {
          winProcessKeyboardMessage(&keyboardController->actionRight, isDown);
        } else if (VKCode == VK_ESCAPE) {
          winProcessKeyboardMessage(&keyboardController->start, isDown);
        } else if (VKCode == VK_SPACE) {
          winProcessKeyboardMessage(&keyboardController->back, isDown);
        } else if (VKCode == 'V') {
          if (isDown) globalMouseOn = !globalMouseOn;
        }
#if RECORD
        else if (VKCode == 'P') {
          if (isDown) globalPause = !globalPause;
        } else if (VKCode == 'L') {
          if (isDown) {
            if (state->inputPlayingIndex == 0) {
              if (state->inputRecordingIndex == 0) {
                winBeginRecordingInput(state, 1);
              } else {
                winEndRecordingInput(state);
                winBeginInputPlayBack(state, 1);
              }
            } else {
              winEndInputPlayBack(state);
            }
          }
        }
#endif
      }

      // 系统按键 ALT + F4
      int32 altKeyWasDown = msg.lParam & (1 << 29);
      if (VKCode == VK_F4 && altKeyWasDown) globalRuning = false;

      break;
    }

    default:
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
      break;
    }
  }
}

// 处理手柄摇杆死区
static float winProcessXInputStickValue(float value, float thumbDeadZone) {
  // 负数最大值
  float maxNegativeThumb = 32768.0f - thumbDeadZone;
  // 正数最大值
  float maxPositiveThumb = 32767.0f - thumbDeadZone;
  // 摇杆实际数值
  float stick = value - thumbDeadZone;

  // 数值更平滑
  if (value < -thumbDeadZone) {
    return stick / maxNegativeThumb;
  } else if (value > thumbDeadZone) {
    return stick / maxPositiveThumb;
  } else {
    return 0;
  }
}

#if HANDMADE_INTERNAL
static void winDebugDrawVertical(win_offscreen_buffer *backbuffer, int xOffset, int top, int bottom, uint32 color) {
  if (top <= 0) top = 0;

  if (bottom > backbuffer->height) bottom = backbuffer->height;

  if ((xOffset >= 0) && (xOffset < backbuffer->width)) {
    uint8 *pixel = (uint8 *)backbuffer->memory + xOffset * backbuffer->bytesPerPixel + top * backbuffer->pitch;
    for (int y = top; y < bottom; ++y) {
      *(uint32 *)pixel = color;
      pixel += backbuffer->pitch;
    }
  }
}

static void winDrawSoundBufferMarker(win_offscreen_buffer *backbuffer,
                                     win_sound_output *soundOutput,
                                     float c, int padX, int top, int bottom,
                                     DWORD value, uint32 color) {
  float XReal32 = (c * (float)value);
  int xOffset = padX + (int)XReal32;
  winDebugDrawVertical(backbuffer, xOffset, top, bottom, color);
}

void winDebugSyncDisplay(win_offscreen_buffer *backbuffer,
                         int markerCount,
                         win_debug_time_marker *markers,
                         int currentMarkerIndex,
                         win_sound_output *soundOutput) {
  int PadX = 16;
  int PadY = 16;

  int LineHeight = 64;

  float C = (float)(backbuffer->width - 2 * PadX) / (float)soundOutput->DSoundBufferSize;
  for (int MarkerIndex = 0;
       MarkerIndex < markerCount;
       ++MarkerIndex) {
    win_debug_time_marker *ThisMarker = &markers[MarkerIndex];
    assert(ThisMarker->outputPlayCursor < soundOutput->DSoundBufferSize);
    assert(ThisMarker->outputWriteCursor < soundOutput->DSoundBufferSize);
    assert(ThisMarker->outputLocation < soundOutput->DSoundBufferSize);
    assert(ThisMarker->outputByteCount < soundOutput->DSoundBufferSize);
    assert(ThisMarker->flipPlayCursor < soundOutput->DSoundBufferSize);
    assert(ThisMarker->flipWriteCursor < soundOutput->DSoundBufferSize);

    DWORD PlayColor = 0xFFFFFFFF;
    DWORD WriteColor = 0xFFFF0000;
    DWORD ExpectedFlipColor = 0xFFFFFF00;
    DWORD PlayWindowColor = 0xFFFF00FF;

    int Top = PadY;
    int Bottom = PadY + LineHeight;
    if (MarkerIndex == currentMarkerIndex) {
      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      int FirstTop = Top;

      winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputPlayCursor, PlayColor);
      winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputWriteCursor, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputLocation, PlayColor);
      winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputLocation + ThisMarker->outputByteCount, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, FirstTop, Bottom, ThisMarker->expectedFlipPlayCursor, ExpectedFlipColor);
    }

    winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->flipPlayCursor, PlayColor);
    winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->flipPlayCursor + 480 * soundOutput->bytesPerSample, PlayWindowColor);
    winDrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->flipWriteCursor, WriteColor);
  }
}
#endif

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;

  switch (message) {
  case WM_SIZE:
    break;

  case WM_DESTROY:
    // TODO handle this as an error - recreate window
    globalRuning = false;
    break;

  case WM_CLOSE:
    // TODO handle this with a message to the user
    globalRuning = false;
    break;

  case WM_ACTIVATEAPP:
    OutputDebugStringA("WM_ACTIVATEAPP\n");
    break;

  case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC deviceContext = BeginPaint(window, &paint);
    auto dimension = getWindowDimension(window);
    winDisplayBufferInWindow(&globalBackBuffer, deviceContext, dimension.width, dimension.height);
    EndPaint(window, &paint);
    break;
  }

  default:
    result = DefWindowProc(window, message, wParam, lParam);
    break;
  }

  return result;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, PWSTR pCmdLine, int showCode) {
  WNDCLASS wc = {};

  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = L"HandmadeHero";

  RegisterClass(&wc);
  HWND window = CreateWindowExW(0,
                                wc.lpszClassName,
                                L"手工英雄",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                0,
                                0,
                                instance,
                                0);
  if (!window) return 0;

  // 设置windows调度粒度(ms)用来更精细地控制睡眠
  constexpr UINT desiredSchedulerMS = 1;
  bool sleepIsGranular = timeBeginPeriod(desiredSchedulerMS) == TIMERR_STRUCT;

  win_state winState = {};

  // 初始化游戏DDL
  // 获取主进程当前路径
  winGetEXEFileName(&winState);

  char sourceGameCodeDLLFullPath[WIN_STATE_FILE_NAME_COUNT];
  winBuildEXEPathFileName(&winState, "handmade.dll", sourceGameCodeDLLFullPath);

  char tempGameCodeDLLFullPath[WIN_STATE_FILE_NAME_COUNT];
  winBuildEXEPathFileName(&winState, "handmade_temp.dll", tempGameCodeDLLFullPath);

  winLoadXInput();

  HDC deviceContext = GetDC(window);
  winResizeDIBSection(&globalBackBuffer, 1920 * 3 / 4, 1080 * 3 / 4);

  // 获取当前设备屏幕刷新率
  int monitorRefresHz = 144;
  int winRefreshRate = GetDeviceCaps(deviceContext, VREFRESH);
  if (winRefreshRate > 1) {
    monitorRefresHz = winRefreshRate;
  }
  // 游戏物理逻辑帧
  int gameUpdateHz = monitorRefresHz / 2;
  // 每帧的时间
  float targetSecondsPerFrame = 1.0f / gameUpdateHz;

  win_sound_output soundOutput = {};
  soundOutput.runingSampleIndex = 0;
  soundOutput.samplesPerSecond = 48000;
  soundOutput.toneVolume = 3000;
  soundOutput.toneHz = 256;
  soundOutput.wavePeroid = soundOutput.samplesPerSecond / 256;
  soundOutput.bytesPerSample = sizeof(int16) * 2;
  soundOutput.DSoundBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
  soundOutput.safetyBytes = (int)((float)soundOutput.DSoundBufferSize / (float)gameUpdateHz / 2.0f);

  winLoadInitDSound(window, soundOutput.samplesPerSecond, soundOutput.DSoundBufferSize);
  winCleanSoundBuffer(&soundOutput);
  globalDSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

  int16 *samples = (int16 *)VirtualAlloc(0, soundOutput.DSoundBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
  LPVOID BaseAddress = (LPVOID)TeraBytes(2);
#else
  LPVOID BaseAddress = 0;
#endif

  game_memory gameMemory = {};
  gameMemory.isInitialized = false;
  gameMemory.permanentStorageSize = MegaBytes(64);
  gameMemory.transientStorageSize = GigaBytes(1);

#if HANDMADE_INTERNAL
  gameMemory.debugPlatformFreeFileMemory = debugPlatformFreeFileMemory;
  gameMemory.debugPlatformReadEntireFile = debugPlatformReadEntireFile;
  gameMemory.debugPlatformWriteEntireFile = debugPlatformWriteEntireFile;
#endif

  winState.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
  // TODO 使用大型页面支持分配内存
  winState.gameMemoryBlock = VirtualAlloc(BaseAddress, winState.totalSize,
                                          MEM_RESERVE | MEM_COMMIT,
                                          PAGE_READWRITE);
  gameMemory.permanentStorage = winState.gameMemoryBlock;

#if RECORD
  for (int replayIndex = 0; replayIndex < arr_length(winState.replayBuffers); ++replayIndex) {
    win_replay_buffer *replayBuffer = &winState.replayBuffers[replayIndex];

    winGetInputFileLocation(&winState, false, replayIndex, replayBuffer->filename);
    replayBuffer->fileHandle = CreateFileA(replayBuffer->filename, GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);

    LARGE_INTEGER MaxSize;
    MaxSize.QuadPart = winState.totalSize;
    replayBuffer->memoryMap = CreateFileMapping(replayBuffer->fileHandle, 0, PAGE_READWRITE, MaxSize.HighPart, MaxSize.LowPart, 0);
    replayBuffer->memoryBlock = MapViewOfFile(replayBuffer->memoryMap, FILE_MAP_ALL_ACCESS, 0, 0, winState.totalSize);
    if (replayBuffer->memoryBlock) {
    } else {
      // TODO(casey): Diagnostic
    }
  }
#endif

  if (!samples || !gameMemory.permanentStorage) {
    // TODO 分配失败
  }

  game_input input[2] = {};
  game_input *newInput = &input[0];
  game_input *oldInput = &input[1];

  DWORD lastPlayCursor = 0;
  bool soundIsValid = false;
  DWORD audioLatencyBytes = 0;
  float audioLatencySeconds = 0;

#if HANDMADE_INTERNAL
  int debugTimeMarkerIndex = 0;
  win_debug_time_marker debugTimeMarkers[30] = {0};
#endif

  globalRuning = true;
  initPerfCountFrequency();

  // 开始计数器
  LARGE_INTEGER lastCounter = winGetWallClock();
  // 游戏更新时间
  LARGE_INTEGER flipWallClock = winGetWallClock();
  // CPU命令计数器
  uint64 lastCycleCount = __rdtsc();

  win_game_code gameCode = winLoadGameCode(sourceGameCodeDLLFullPath, tempGameCodeDLLFullPath);

  while (globalRuning) {
    // 检测DLL文件更新重新读取DLL文件
    FILETIME newDLLWriteTime = winGetLastWriteTime(sourceGameCodeDLLFullPath);
    if (CompareFileTime(&newDLLWriteTime, &gameCode.DLLLastWriteTime) != 0) {
      winUnloadGameCode(&gameCode);
      gameCode = winLoadGameCode(sourceGameCodeDLLFullPath, tempGameCodeDLLFullPath);
    }

    newInput->dtForFrame = targetSecondsPerFrame;

    // 清空键盘控制器状态
    game_controller_input *oldController = &oldInput->controller[0];
    game_controller_input *newController = &newInput->controller[0];
    memset(newController, 0, sizeof(*newController));
    // 维持键盘前一帧的状态
    for (int buttonIndex = 0; buttonIndex < arr_length(newController->Button); ++buttonIndex) {
      newController->Button[buttonIndex].endDown = oldController->Button[buttonIndex].endDown;
    }

    winProcessPendingMessage(&winState, newController);
    newController->isConnected = true;

    // 启动鼠标检测
    if (globalMouseOn) {
      POINT mouseP;
      GetCursorPos(&mouseP);
      ScreenToClient(window, &mouseP);
      newInput->mouseX = mouseP.x;
      newInput->mouseY = mouseP.y;
      newInput->mouseZ = 0;
      winProcessKeyboardMessage(&newInput->mouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
      winProcessKeyboardMessage(&newInput->mouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
      winProcessKeyboardMessage(&newInput->mouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
      winProcessKeyboardMessage(&newInput->mouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
      winProcessKeyboardMessage(&newInput->mouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));
    }

    int minControllerCount = min(arr_length(input->controller) - 1, XUSER_MAX_COUNT);
    // 第零个控制器是键盘
    for (DWORD i = 0; i < minControllerCount; ++i) {
      XINPUT_STATE state = {};

      int controllerIndex = i + 1;
      game_controller_input *oldController = &oldInput->controller[controllerIndex];
      game_controller_input *newController = &newInput->controller[controllerIndex];

      if (XInputGetState(i, &state) != ERROR_SUCCESS) {
        // NOTE The controller is not available
        newController->isConnected = false;
        continue;
      }

      XINPUT_GAMEPAD *pad = &state.Gamepad;
      // 维持上一帧的模拟输入类型
      newController->isAnalog = oldController->isAnalog;

      // 获取摇杆值
      newController->stickAverageY = winProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      newController->stickAverageX = winProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

      if (newController->stickAverageY != 0.0f || newController->stickAverageX != 0.0f) {
        newController->isAnalog = true;
      }

      // 摇杆模拟控制方向键
      float threshold = 0.5f;
      if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        newController->stickAverageY = 1.0f;
        newController->isAnalog = false;
      }
      if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        newController->stickAverageY = -1.0f;
        newController->isAnalog = false;
      }
      if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        newController->stickAverageX = -1.0f;
        newController->isAnalog = false;
      }
      if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        newController->stickAverageX = 1.0f;
        newController->isAnalog = false;
      }

      winProcessXInputDigitalButton(&newController->moveUp, &oldController->moveUp, oldController->stickAverageY > threshold, 1);
      winProcessXInputDigitalButton(&newController->moveDown, &oldController->moveDown, oldController->stickAverageY < -threshold, 1);
      winProcessXInputDigitalButton(&newController->moveLeft, &oldController->moveLeft, oldController->stickAverageX < -threshold, 1);
      winProcessXInputDigitalButton(&newController->moveRight, &oldController->moveRight, oldController->stickAverageX > threshold, 1);

      winProcessXInputDigitalButton(&newController->actionUp, &oldController->moveUp, pad->wButtons, XINPUT_GAMEPAD_Y);
      winProcessXInputDigitalButton(&newController->actionDown, &oldController->moveDown, pad->wButtons, XINPUT_GAMEPAD_A);
      winProcessXInputDigitalButton(&newController->actionLeft, &oldController->moveLeft, pad->wButtons, XINPUT_GAMEPAD_X);
      winProcessXInputDigitalButton(&newController->actionRight, &oldController->moveRight, pad->wButtons, XINPUT_GAMEPAD_B);

      winProcessXInputDigitalButton(&newController->leftShoulder, &oldController->leftShoulder, pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER);
      winProcessXInputDigitalButton(&newController->rightShoulder, &oldController->rightShoulder, pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER);

      winProcessXInputDigitalButton(&newController->start, &oldController->start, pad->wButtons, XINPUT_GAMEPAD_START);
      winProcessXInputDigitalButton(&newController->back, &oldController->back, pad->wButtons, XINPUT_GAMEPAD_BACK);

      newController->isConnected = true;
      if (pad->wButtons & XINPUT_GAMEPAD_B) globalRuning = false;
    }

    if (winState.inputRecordingIndex) {
      winRecordInput(&winState, newInput);
    }

    if (winState.playbackHandle) {
      winPlayBackInput(&winState, newInput);
    }

    // 暂停
    if (globalPause) continue;

#if 0
    // 手柄震动
    XINPUT_VIBRATION vibration = {60000, 60000};
    XInputSetState(0, &vibration);
#endif

    // 线程上下文
    thread_context thread = {};

    game_offscreen_buffer offscreen_buffer = {};
    offscreen_buffer.memory = globalBackBuffer.memory;
    offscreen_buffer.width = globalBackBuffer.width;
    offscreen_buffer.height = globalBackBuffer.height;
    offscreen_buffer.pitch = globalBackBuffer.pitch;
    offscreen_buffer.bytesPerPixel = globalBackBuffer.bytesPerPixel;
    if (gameCode.updateAndRender) {
      gameCode.updateAndRender(&thread, &gameMemory, newInput, &offscreen_buffer);
    }

    LARGE_INTEGER audioWallClock = winGetWallClock();
    float fromBeginToAudioSeconds = winGetSecondsElapsed(flipWallClock, audioWallClock);

    // 控制音频延迟
    DWORD playCursor;  // 当前播放光标
    DWORD writeCursor; // 可写入光标
    if (globalDSoundBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK) {
      // 重置音频更新索引为音频写入光标
      if (!soundIsValid) {
        soundOutput.runingSampleIndex = writeCursor / soundOutput.bytesPerSample;
        soundIsValid = true;
      }

      // 加锁写入音频位置
      DWORD byteToLock = (soundOutput.runingSampleIndex * soundOutput.bytesPerSample) % soundOutput.DSoundBufferSize;
      // 预期写入字节
      DWORD expectedSoundBytesPerFrame = (soundOutput.samplesPerSecond * soundOutput.bytesPerSample) / gameUpdateHz;
      // 每帧剩余时间
      float secondsLeftUntilFlip = targetSecondsPerFrame - fromBeginToAudioSeconds;
      // 预期剩余时间可以写入的字节
      DWORD expectedBytesUntilFlip = (DWORD)((secondsLeftUntilFlip / targetSecondsPerFrame) * (float)expectedSoundBytesPerFrame);
      // 预期写入的边界
      DWORD expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;

      // 安全光标
      DWORD safeWriteCursor = writeCursor;
      if (safeWriteCursor < playCursor) {
        safeWriteCursor += soundOutput.DSoundBufferSize;
      }
      assert(safeWriteCursor >= playCursor);
      safeWriteCursor += soundOutput.safetyBytes;

      bool audioCardIsLowLatency = safeWriteCursor < expectedFrameBoundaryByte;

      DWORD targetCursor = 0;
      if (audioCardIsLowLatency) {
        targetCursor = expectedFrameBoundaryByte + expectedSoundBytesPerFrame;
      } else {
        targetCursor = writeCursor + expectedSoundBytesPerFrame + soundOutput.safetyBytes;
      }
      targetCursor = targetCursor % soundOutput.DSoundBufferSize;

      DWORD bytesToWrite = 0;
      if (byteToLock > targetCursor) {
        bytesToWrite = soundOutput.DSoundBufferSize - byteToLock;
        bytesToWrite += targetCursor;
      } else {
        bytesToWrite = targetCursor - byteToLock;
      }

      game_sound_output_buffer soundBuffer = {};
      soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
      soundBuffer.bufferSize = bytesToWrite / soundOutput.bytesPerSample;
      soundBuffer.buffer = samples;
      soundBuffer.toneVolume = soundOutput.toneVolume;
      if (gameCode.getSoundSamples) {
        gameCode.getSoundSamples(&thread, &gameMemory, soundBuffer);
      }

#if HANDMADE_INTERNAL
      win_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkerIndex];
      marker->outputPlayCursor = playCursor;
      marker->outputWriteCursor = writeCursor;
      marker->outputLocation = byteToLock;
      marker->outputByteCount = bytesToWrite;
      marker->expectedFlipPlayCursor = expectedFrameBoundaryByte;

      DWORD unwrappedWriteCursor = writeCursor;
      if (unwrappedWriteCursor < playCursor) {
        unwrappedWriteCursor += soundOutput.DSoundBufferSize;
      }
      audioLatencyBytes = unwrappedWriteCursor - playCursor;
      audioLatencySeconds = ((float)audioLatencyBytes / (float)soundOutput.bytesPerSample) / (float)soundOutput.samplesPerSecond;
      printf("BTL:%lu TC:%lu BTW:%lu - PC:%lu WC:%lu DELTA:%lu (%fs)\n",
             byteToLock, targetCursor, bytesToWrite, playCursor, writeCursor, audioLatencyBytes, audioLatencySeconds);
#endif

      winFillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
    } else {
      soundIsValid = false;
    }

    LARGE_INTEGER endCounter = winGetWallClock();
    // 控制物理逻辑帧
    float secondsElapsedForWork = winGetSecondsElapsed(lastCounter, endCounter);
    if (secondsElapsedForWork < targetSecondsPerFrame) {
      if (sleepIsGranular) {
        DWORD sleepMS = (DWORD)(1000.0f * (targetSecondsPerFrame - secondsElapsedForWork));
        if (sleepMS > 0) Sleep(sleepMS);
      }

      float testSecondsElapsedForFrame = winGetSecondsElapsed(lastCounter, winGetWallClock());
      if (testSecondsElapsedForFrame < targetSecondsPerFrame) {
        // TODO LOG MISSED SLEEP HERE
      }

      while (secondsElapsedForWork < targetSecondsPerFrame) {
        secondsElapsedForWork = winGetSecondsElapsed(lastCounter, winGetWallClock());
      }
    } else {
      // TODO 超过一帧 Logging
    }

    uint64 endCycleCount = __rdtsc();
    uint64 cycleCounterElapsed = endCycleCount - lastCycleCount;
    endCounter = winGetWallClock();

#if HANDMADE_INTERNAL
    uint64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
    // 计算一帧循环耗时毫秒数
    float MSPerFrame = (float)counterElapsed * 1000.f / (float)globalPerfCountFrequency;
    // FPS = 每秒计数器递增的次数 / 一帧循环所经过的计数器刻度数
    uint64 FPS = globalPerfCountFrequency / counterElapsed;
    // 一帧CPU执行了多少万条指令
    float MCPF = (float)cycleCounterElapsed / 10000.f;
    printf("MSPerFrame/FPS/MCPF => %.2fms / %lluFPS / %.2f \n", MSPerFrame, FPS, MCPF);
#endif

#if HANDMADE_INTERNAL
    winDebugSyncDisplay(&globalBackBuffer, arr_length(debugTimeMarkers), debugTimeMarkers, debugTimeMarkerIndex - 1, &soundOutput);
#endif

    auto dimension = getWindowDimension(window);
    winDisplayBufferInWindow(&globalBackBuffer, deviceContext, dimension.width, dimension.height);
    flipWallClock = winGetWallClock();

#if HANDMADE_INTERNAL
    // NOTE: This is debug code
    {
      DWORD playCursor;
      DWORD writeCursor;
      if (globalDSoundBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK) {
        assert(debugTimeMarkerIndex < arr_length(debugTimeMarkers));
        win_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkerIndex];
        marker->flipPlayCursor = playCursor;
        marker->flipWriteCursor = writeCursor;
      }
    }
#endif

    lastCounter = endCounter;
    lastCycleCount = endCycleCount;

    game_input *temp = newInput;
    newInput = oldInput;
    oldInput = temp;

#if HANDMADE_INTERNAL
    ++debugTimeMarkerIndex;
    if (debugTimeMarkerIndex == arr_length(debugTimeMarkers)) {
      debugTimeMarkerIndex = 0;
    }
#endif
  }

  return 0;
}
