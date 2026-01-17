#ifndef UNICODE
#define UNICODE
#endif

#include <stdio.h>
#include "windows.h"

static bool globalRuning;
static bool globalPause;
static win64_offscreen_buffer globalBackBuffer;
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

#ifdef HANDMADE_INTERNAL
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
    debugPlatformFreeFileMemory(result.contents);
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

// 动态读取加载游戏相关代码
static win64_game_code win64LoadGameCode() {
  win64_game_code result = {};

  CopyFileA("handmade.dll", "handmade_temp.dll", FALSE);
  result.gameCodeDLL = LoadLibraryA("handmade_temp.dll");
  if (result.gameCodeDLL) {
    result.updateAndRender = (game_update_and_render *)GetProcAddress(result.gameCodeDLL, "gameUpdateAndRender");
    result.getSoundSamples = (game_get_sound_samples *)GetProcAddress(result.gameCodeDLL, "gameGetSoundSamples");
    result.isValid = result.updateAndRender && result.getSoundSamples;
  }

  if (!result.isValid) {
    result.updateAndRender = gameUpdateAndRenderStub;
    result.getSoundSamples = gameGetSoundSamplesStub;
  }

  return result;
}

// 动态读取卸载游戏相关代码
static void win64UnloadGameCode(win64_game_code *gameCode) {
  if (gameCode->gameCodeDLL) {
    FreeLibrary(gameCode->gameCodeDLL);
    gameCode->gameCodeDLL = 0;
  }

  gameCode->isValid = false;
  gameCode->updateAndRender = gameUpdateAndRenderStub;
  gameCode->getSoundSamples = gameGetSoundSamplesStub;
}

// 初始化音频API
static void win64LoadInitDSound(HWND window, int32 samplesPerSecond, int32 bufferSize) {
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
static void win64LoadXInput() {
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
  if (!XInputLibrary) XInputLibrary = LoadLibraryA("xinput1_3.dll");

  if (XInputLibrary) {
    XInputGetState_ = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState_ = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

static win64_window_dimension getWindowDimension(HWND window);
static void win64ResizeDIBSection(win64_offscreen_buffer *buffer, int width, int height);
static void win64DisplayBufferInWindow(win64_offscreen_buffer *buffer, HDC deviceContext, int windowWidth, int windowHeight);
static void win64FillSoundBuffer(win64_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_sound_output_buffer *sound_output_buffer);
static void win64CleanSoundBuffer(win64_sound_output *soundOutput);

static void initPerfCountFrequency() {
  LARGE_INTEGER lpFrequency;
  QueryPerformanceFrequency(&lpFrequency);
  globalPerfCountFrequency = lpFrequency.QuadPart;
}

static LARGE_INTEGER win64GetWallClock() {
  LARGE_INTEGER result;
  QueryPerformanceCounter(&result);
  return result;
}

static uint64 win64GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
  uint64 counterElapsed = end.QuadPart - start.QuadPart;
  uint64 secondsElapsedForWork = (counterElapsed * 1000) / globalPerfCountFrequency;
  return secondsElapsedForWork;
}

// 更新手柄按键状态并计算变化数
static void win64ProcessXInputDigitalButton(game_button_state *newState, game_button_state *oldState, WORD wButtons, DWORD buttonBit) {
  newState->endDown = (wButtons & buttonBit) == buttonBit;
  newState->halfTransitionCount = newState->endDown != oldState->endDown;
}

// 更新键盘状态并计算变化数
static void win64ProcessKeyboardMessage(game_button_state *newState, bool isDown) {
  // assert(newState->endDown != isDown); TODO 中文输入法拼音异常
  newState->endDown = isDown;
  ++newState->halfTransitionCount;
}

// 处理windows消息
static void win64ProcessPendingMessage(game_controller_input *keyboardController) {
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
          win64ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
        } else if (VKCode == 'A') {
          win64ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
        } else if (VKCode == 'S') {
          win64ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
        } else if (VKCode == 'D') {
          win64ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
        } else if (VKCode == 'Q') {
          win64ProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
        } else if (VKCode == 'E') {
          win64ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
        } else if (VKCode == VK_UP) {
          win64ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
        } else if (VKCode == VK_LEFT) {
          win64ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
        } else if (VKCode == VK_DOWN) {
          win64ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
        } else if (VKCode == VK_RIGHT) {
          win64ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
        } else if (VKCode == VK_ESCAPE) {
          win64ProcessKeyboardMessage(&keyboardController->start, isDown);
        } else if (VKCode == VK_SPACE) {
          win64ProcessKeyboardMessage(&keyboardController->back, isDown);
        }
#ifdef HANDMADE_INTERNAL
        else if (VKCode == 'P') {
          if (isDown) globalPause = !globalPause;
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
static float win64ProcessXInputStickValue(float value, float thumbDeadZone) {
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

static void win64DebugDrawVertical(win64_offscreen_buffer *backbuffer, int xOffset, int top, int bottom, uint32 color) {
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

static void win64DrawSoundBufferMarker(win64_offscreen_buffer *backbuffer,
                                       win64_sound_output *soundOutput,
                                       float c, int padX, int top, int bottom,
                                       DWORD value, uint32 color) {
  float XReal32 = (c * (float)value);
  int xOffset = padX + (int)XReal32;
  win64DebugDrawVertical(backbuffer, xOffset, top, bottom, color);
}

void win64DebugSyncDisplay(win64_offscreen_buffer *backbuffer,
                           int markerCount,
                           win64_debug_time_marker *markers,
                           int currentMarkerIndex,
                           win64_sound_output *soundOutput,
                           float targetSecondsPerFrame) {
  int PadX = 16;
  int PadY = 16;

  int LineHeight = 64;

  float C = (float)(backbuffer->width - 2 * PadX) / (float)soundOutput->DSoundBufferSize;
  for (int MarkerIndex = 0;
       MarkerIndex < markerCount;
       ++MarkerIndex) {
    win64_debug_time_marker *ThisMarker = &markers[MarkerIndex];
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

      win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputPlayCursor, PlayColor);
      win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputWriteCursor, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputLocation, PlayColor);
      win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->outputLocation + ThisMarker->outputByteCount, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, FirstTop, Bottom, ThisMarker->expectedFlipPlayCursor, ExpectedFlipColor);
    }

    win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->flipPlayCursor, PlayColor);
    win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->flipPlayCursor + 480 * soundOutput->bytesPerSample, PlayWindowColor);
    win64DrawSoundBufferMarker(backbuffer, soundOutput, C, PadX, Top, Bottom, ThisMarker->flipWriteCursor, WriteColor);
  }
}

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

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
  // 显示器帧数
  constexpr uint16 monitorRefresHz = 144;
  // 游戏物理逻辑帧
  constexpr uint16 gameUpdateHz = 60;
  // 每帧的时间
  constexpr uint16 targetElapsedPerFrame = 1000 / gameUpdateHz;
  // 设置windows调度粒度(ms)用来更精细地控制睡眠
  constexpr UINT desiredSchedulerMS = 1;
  bool sleepIsGranular = timeBeginPeriod(desiredSchedulerMS) == TIMERR_STRUCT;

  win64LoadXInput();

  HDC deviceContext = GetDC(window);
  win64ResizeDIBSection(&globalBackBuffer, 1280, 720);

  win64_sound_output soundOutput = {};
  soundOutput.runingSampleIndex = 0;
  soundOutput.samplesPerSecond = 48000;
  soundOutput.toneVolume = 3000;
  soundOutput.toneHz = 256;
  soundOutput.wavePeroid = soundOutput.samplesPerSecond / 256;
  soundOutput.bytesPerSample = sizeof(int16) * 2;
  soundOutput.DSoundBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
  soundOutput.latencySampleCount = 3 * (soundOutput.samplesPerSecond / gameUpdateHz);
  soundOutput.safetyBytes = (soundOutput.samplesPerSecond * soundOutput.bytesPerSample / gameUpdateHz) / 3;

  win64LoadInitDSound(window, soundOutput.samplesPerSecond, soundOutput.DSoundBufferSize);
  win64CleanSoundBuffer(&soundOutput);
  globalDSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

  int16 *samples = (int16 *)VirtualAlloc(0, soundOutput.DSoundBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  game_memory gameMemory = {};
  gameMemory.isInitialized = false;
  gameMemory.permanentStorageSize = GigaBytes(4);
  gameMemory.permanentStorage = VirtualAlloc(0, gameMemory.permanentStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  gameMemory.debugPlatformFreeFileMemory = debugPlatformFreeFileMemory;
  gameMemory.debugPlatformReadEntireFile = debugPlatformReadEntireFile;
  gameMemory.debugPlatformWriteEntireFile = debugPlatformWriteEntireFile;

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

  int debugTimeMarkerIndex = 0;
  win64_debug_time_marker debugTimeMarkers[gameUpdateHz / 4] = {0};

  globalRuning = true;
  initPerfCountFrequency();

  // 开始计数器
  LARGE_INTEGER lastCounter = win64GetWallClock();
  // 游戏更新时间
  LARGE_INTEGER flipWallClock = win64GetWallClock();
  // CPU命令计数器
  uint64 lastCycleCount = __rdtsc();

  win64_game_code gameCode = win64LoadGameCode();
  uint32 loadGameCodeCount = 0;

  while (globalRuning) {
    // 每三秒热更新游戏代码一次
    if (loadGameCodeCount++ > gameUpdateHz * 3) {
      win64UnloadGameCode(&gameCode);
      gameCode = win64LoadGameCode();
      loadGameCodeCount = 0;
    }

    // 清空键盘控制器状态
    game_controller_input *oldController = &oldInput->controller[0];
    game_controller_input *newController = &newInput->controller[0];
    memset(newController, 0, sizeof(*newController));
    // 维持键盘前一帧的状态
    for (int buttonIndex = 0; buttonIndex < arr_length(newController->Button); ++buttonIndex) {
      newController->Button[buttonIndex].endDown = oldController->Button[buttonIndex].endDown;
    }

    win64ProcessPendingMessage(newController);
    newController->isConnected = true;

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
      newController->isAnalog = true;

      // 获取摇杆值
      newController->stickAverageY = win64ProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      newController->stickAverageX = win64ProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
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

      win64ProcessXInputDigitalButton(&newController->moveUp, &oldController->moveUp, oldController->stickAverageY > threshold, 1);
      win64ProcessXInputDigitalButton(&newController->moveDown, &oldController->moveDown, oldController->stickAverageY < -threshold, 1);
      win64ProcessXInputDigitalButton(&newController->moveLeft, &oldController->moveLeft, oldController->stickAverageX < -threshold, 1);
      win64ProcessXInputDigitalButton(&newController->moveRight, &oldController->moveRight, oldController->stickAverageX > threshold, 1);

      win64ProcessXInputDigitalButton(&newController->actionUp, &oldController->moveUp, pad->wButtons, XINPUT_GAMEPAD_Y);
      win64ProcessXInputDigitalButton(&newController->actionDown, &oldController->moveDown, pad->wButtons, XINPUT_GAMEPAD_A);
      win64ProcessXInputDigitalButton(&newController->actionLeft, &oldController->moveLeft, pad->wButtons, XINPUT_GAMEPAD_X);
      win64ProcessXInputDigitalButton(&newController->actionRight, &oldController->moveRight, pad->wButtons, XINPUT_GAMEPAD_B);

      win64ProcessXInputDigitalButton(&newController->leftShoulder, &oldController->leftShoulder, pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER);
      win64ProcessXInputDigitalButton(&newController->rightShoulder, &oldController->rightShoulder, pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER);

      win64ProcessXInputDigitalButton(&newController->start, &oldController->start, pad->wButtons, XINPUT_GAMEPAD_START);
      win64ProcessXInputDigitalButton(&newController->back, &oldController->back, pad->wButtons, XINPUT_GAMEPAD_BACK);

      newController->isConnected = true;
      if (pad->wButtons & XINPUT_GAMEPAD_B) globalRuning = false;
    }

    // 暂停
    if (globalPause) continue;

#if 0
    // 手柄震动
    XINPUT_VIBRATION vibration = {60000, 60000};
    XInputSetState(0, &vibration);
#endif

    game_offscreen_buffer offscreen_buffer = {};
    offscreen_buffer.memory = globalBackBuffer.memory;
    offscreen_buffer.width = globalBackBuffer.width;
    offscreen_buffer.height = globalBackBuffer.height;
    offscreen_buffer.bytesPerPixel = globalBackBuffer.bytesPerPixel;
    gameCode.updateAndRender(&gameMemory, newInput, offscreen_buffer);

    LARGE_INTEGER audioWallClock = win64GetWallClock();
    DWORD fromBeginToAudioSeconds = win64GetSecondsElapsed(flipWallClock, audioWallClock);

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
      DWORD secondsLeftUntilFlip = targetElapsedPerFrame - fromBeginToAudioSeconds;
      // 预期剩余时间可以写入的字节
      DWORD expectedBytesUntilFlip = (DWORD)(((float)secondsLeftUntilFlip / (float)targetElapsedPerFrame) * (float)expectedSoundBytesPerFrame);
      // 预期写入的边界
      DWORD expectedFrameBoundaryByte = playCursor + expectedSoundBytesPerFrame;

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
      gameCode.getSoundSamples(&gameMemory, soundBuffer);

#ifdef HANDMADE_INTERNAL
      win64_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkerIndex];
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

      win64FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
    } else {
      soundIsValid = false;
    }

    LARGE_INTEGER endCounter = win64GetWallClock();
    // 控制物理逻辑帧
    uint64 secondsElapsedForWork = win64GetSecondsElapsed(lastCounter, endCounter);
    if (secondsElapsedForWork < targetElapsedPerFrame) {
      if (sleepIsGranular) {
        DWORD sleepMS = targetElapsedPerFrame - secondsElapsedForWork;
        if (sleepMS > 0) Sleep(sleepMS);
      }

      uint64 testSecondsElapsedForFrame = win64GetSecondsElapsed(lastCounter, win64GetWallClock());
      if (testSecondsElapsedForFrame < targetElapsedPerFrame) {
        // TODO LOG MISSED SLEEP HERE
      }

      while (secondsElapsedForWork < targetElapsedPerFrame) {
        secondsElapsedForWork = win64GetSecondsElapsed(lastCounter, win64GetWallClock());
      }
    } else {
      // TODO 超过一帧 Logging
    }

    uint64 endCycleCount = __rdtsc();
    uint64 cycleCounterElapsed = endCycleCount - lastCycleCount;

    endCounter = win64GetWallClock();
    uint64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
    // 计算一帧循环耗时毫秒数
    float MSPerFrame = (float)counterElapsed * 1000.f / (float)globalPerfCountFrequency;
    // FPS = 每秒计数器递增的次数 / 一帧循环所经过的计数器刻度数
    uint64 FPS = globalPerfCountFrequency / counterElapsed;
    // 一帧CPU执行了多少万条指令
    float MCPF = (float)cycleCounterElapsed / 10000.f;
    printf("MSPerFrame/FPS/MCPF => %.2fms / %lluFPS / %.2f \n", MSPerFrame, FPS, MCPF);

#ifdef HANDMADE_INTERNAL
    win64DebugSyncDisplay(&globalBackBuffer, arr_length(debugTimeMarkers), debugTimeMarkers, debugTimeMarkerIndex - 1, &soundOutput, targetElapsedPerFrame);
#endif

    auto dimension = getWindowDimension(window);
    win64DisplayBufferInWindow(&globalBackBuffer, deviceContext, dimension.width, dimension.height);
    flipWallClock = win64GetWallClock();

#ifdef HANDMADE_INTERNAL
    // NOTE: This is debug code
    {
      DWORD playCursor;
      DWORD writeCursor;
      if (globalDSoundBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK) {
        assert(debugTimeMarkerIndex < arr_length(debugTimeMarkers));
        win64_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkerIndex];
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

#ifdef HANDMADE_INTERNAL
    ++debugTimeMarkerIndex;
    if (debugTimeMarkerIndex == arr_length(debugTimeMarkers)) {
      debugTimeMarkerIndex = 0;
    }
#endif
  }

  return 0;
}

static win64_window_dimension getWindowDimension(HWND window) {
  win64_window_dimension result;

  RECT clientRect;
  GetClientRect(window, &clientRect);
  result.width = clientRect.right - clientRect.left;
  result.height = clientRect.bottom - clientRect.top;

  return result;
}

static void win64DisplayBufferInWindow(win64_offscreen_buffer *buffer,
                                       HDC deviceContext,
                                       int windowWidth, int windowHeight) {
  StretchDIBits(deviceContext,
                0, 0, windowWidth, windowHeight,
                0, 0, buffer->width, buffer->height,
                buffer->memory,
                &buffer->info,
                DIB_RGB_COLORS, SRCCOPY);
}

static void win64ResizeDIBSection(win64_offscreen_buffer *buffer, int width, int height) {

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

static void win64CleanSoundBuffer(win64_sound_output *soundOutput) {
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

static void win64FillSoundBuffer(win64_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_sound_output_buffer *sound_output_buffer) {
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
    win64DisplayBufferInWindow(&globalBackBuffer, deviceContext, dimension.width, dimension.height);
    EndPaint(window, &paint);
    break;
  }

  default:
    result = DefWindowProc(window, message, wParam, lParam);
    break;
  }

  return result;
}
