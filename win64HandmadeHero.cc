#ifndef UNICODE
#define UNICODE
#endif

#include "windows.h"

static bool global_runing;
static win64_offscreen_buffer globalBackBuffer;
static LPDIRECTSOUNDBUFFER globalDSoundBuffer;

// 优雅降级
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState) WIN_NOEXCEPT
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(xInputGetState) { return 0; }
static x_input_get_state *XInputGetState_ = xInputGetState;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration) WIN_NOEXCEPT
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(xInputSetState) { return 0; }
static x_input_set_state *XInputSetState_ = xInputSetState;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
DIRECT_SOUND_CREATE(directSoundCreate) { return 0; }
static direct_sound_create *DirectSoundCreate_ = directSoundCreate;
#define DirectSoundCreate DirectSoundCreate_

void DEBUGPlatformFreeMemory(void *memory) {
  if (memory) VirtualFree(memory, 0, 0);
}

debug_read_file_result DEBUGPlatformReadFile(char *filename) {
  debug_read_file_result result = {0, nullptr};
  HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (fileHandle == INVALID_HANDLE_VALUE) return result;

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(fileHandle, &fileSize)) return result;

  result.memory = VirtualAlloc(0, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!result.memory) {
    // TODO 读取文件内存分配失败
  }

  DWORD bytesRead;
  result.fileSize = saveCastUint64(fileSize.QuadPart);
  if (ReadFile(fileHandle, result.memory, result.fileSize, &bytesRead, 0) && result.fileSize == bytesRead) {
    // NOTE 读取成功
  } else {
    DEBUGPlatformFreeMemory(result.memory);
    result.memory = nullptr;
  }

  CloseHandle(fileHandle);
  return result;
}

bool DEBUGPlatformWriteEntireFile(char *filename, uint32 memorySize, void *memory) {
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

static void win64ProcessXInputDigitalButton(game_button_state *newState, game_button_state *oldState, WORD wButtons, DWORD buttonBit) {
  newState->endDown = (wButtons & buttonBit) == buttonBit;
  newState->halfTransitionCount = newState->endDown != oldState->endDown;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

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
  soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;

  win64LoadInitDSound(window, soundOutput.samplesPerSecond, soundOutput.DSoundBufferSize);
  win64CleanSoundBuffer(&soundOutput);
  globalDSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

  int16 *samples = (int16 *)VirtualAlloc(0, soundOutput.DSoundBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  game_memory gameMemory = {};
  gameMemory.isInitialized = false;
  gameMemory.permanentStorageSize = GigaBytes(4);
  gameMemory.permanentStorage = VirtualAlloc(0, gameMemory.permanentStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  if (!samples || !gameMemory.permanentStorage) {
    // TODO 分配失败
  }

  game_input input[2] = {};
  game_input *newInput = &input[0];
  game_input *oldInput = &input[1];

  global_runing = true;

#if 0
  LARGE_INTEGER lpFrequency;
  QueryPerformanceFrequency(&lpFrequency);
  // 每秒计数器递增的次数
  LONGLONG perfCountFrequency = lpFrequency.QuadPart;

  // 计数器刻度数
  LARGE_INTEGER lastCounter;
  QueryPerformanceCounter(&lastCounter);
  // CPU命令计数器
  uint64 lastCycleCount = __rdtsc();
#endif

  while (global_runing) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) global_runing = false;

      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    int maxControllerCount = max(arr_length(input->controller), XUSER_MAX_COUNT);
    for (DWORD i = 0; i < maxControllerCount; i++) {
      game_controller_input *oldController = &newInput->controller[i];
      game_controller_input *newController = &oldInput->controller[i];

      XINPUT_STATE state;
      ZeroMemory(&state, sizeof(XINPUT_STATE));

      // Simply get the state of the controller from XInput.
      DWORD dwResult = XInputGetState(i, &state);

      if (dwResult == ERROR_SUCCESS) {
        XINPUT_GAMEPAD *pad = &state.Gamepad;

        bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
        bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
        bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
        bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
        bool start = pad->wButtons & XINPUT_GAMEPAD_START;

        win64ProcessXInputDigitalButton(&oldController->down, &oldController->down, pad->wButtons, XINPUT_GAMEPAD_A);
        win64ProcessXInputDigitalButton(&oldController->right, &oldController->right, pad->wButtons, XINPUT_GAMEPAD_B);
        win64ProcessXInputDigitalButton(&oldController->left, &oldController->left, pad->wButtons, XINPUT_GAMEPAD_X);
        win64ProcessXInputDigitalButton(&oldController->up, &oldController->up, pad->wButtons, XINPUT_GAMEPAD_Y);
        win64ProcessXInputDigitalButton(&oldController->leftShoulder, &oldController->leftShoulder, pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER);
        win64ProcessXInputDigitalButton(&oldController->rightShoulder, &oldController->rightShoulder, pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER);

        newInput->controller[i].isAnalog = true;
        newInput->controller[i].EndX = (float)pad->sThumbRX / 32767.f;
        newInput->controller[i].EndY = (float)pad->sThumbRY / 32767.f;
        newInput->controller[i].minX = min(newInput->controller[i].minX, oldInput->controller[i].minX);
        newInput->controller[i].minY = min(newInput->controller[i].minY, oldInput->controller[i].minY);

        // TODO 处理控制器死区 XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
        int16 stickX = pad->sThumbRX;
        int16 stickY = pad->sThumbRY;

        bool B = pad->wButtons & XINPUT_GAMEPAD_B;
        if (B) global_runing = false;

      } else {
        // NOTE The controller is not available
      }
    }

#if 0
    // 手柄震动
    XINPUT_VIBRATION vibration = {60000, 60000};
    XInputSetState(0, &vibration);
#endif

    DWORD playCursor;  // 当前播放光标
    DWORD writeCursor; // 可写入光标
    DWORD byteToLock;
    DWORD bytesToWrite;
    bool soundIsValid;
    if (SUCCEEDED(globalDSoundBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
      byteToLock = (soundOutput.runingSampleIndex * soundOutput.bytesPerSample) % soundOutput.DSoundBufferSize;
      DWORD targetCursor =
          (playCursor + soundOutput.latencySampleCount * soundOutput.bytesPerSample) % soundOutput.DSoundBufferSize;
      bytesToWrite =
          byteToLock > targetCursor ? soundOutput.DSoundBufferSize - byteToLock + targetCursor : targetCursor - byteToLock;
      soundIsValid = true;
    }

    game_offscreen_buffer offscreen_buffer = {};
    offscreen_buffer.memory = globalBackBuffer.memory;
    offscreen_buffer.width = globalBackBuffer.width;
    offscreen_buffer.height = globalBackBuffer.height;
    offscreen_buffer.bytesPerPixel = globalBackBuffer.bytesPerPixel;

    game_sound_output_buffer sound_output_buffer = {};
    sound_output_buffer.buffer = (void *)samples;
    sound_output_buffer.bufferSize = (int)(bytesToWrite / soundOutput.bytesPerSample);
    sound_output_buffer.samplesPerSecond = soundOutput.samplesPerSecond;
    sound_output_buffer.toneVolume = soundOutput.toneVolume;

    gameUpdateAndRender(&gameMemory, newInput, offscreen_buffer, sound_output_buffer);

    if (soundIsValid) win64FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &sound_output_buffer);
    auto [width, height] = getWindowDimension(window);
    win64DisplayBufferInWindow(&globalBackBuffer, deviceContext, width, height);

#if 0
    uint64 endCycleCount = __rdtsc();
    uint64 cycleCounterElapsed = endCycleCount - lastCycleCount;

    LARGE_INTEGER endCounter;
    QueryPerformanceCounter(&endCounter);
    // 一帧循环所经过的计数器刻度数
    uint64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
    // 计算一帧循环耗时毫秒数
    float MSPerFrame = (float)counterElapsed * 1000.f / (float)perfCountFrequency;
    // FPS = 每秒计数器递增的次数 / 一帧循环所经过的计数器刻度数
    uint64 FPS = perfCountFrequency / counterElapsed;
    // 一帧CPU执行了多少万条指令
    float MCPF = (float)cycleCounterElapsed / 10000.f;

    char buffer[256] = {0};
    sprintf(buffer, "MSPerFrame/FPS/MCPF => %.2fms / %lluFPS / %.2f \n", MSPerFrame, FPS, MCPF);
    OutputDebugStringA(buffer);

    lastCounter = endCounter;
    lastCycleCount = endCycleCount;
#endif

    game_input *temp = newInput;
    newInput = oldInput;
    oldInput = temp;
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
    global_runing = false;
    break;
  case WM_CLOSE:
    // TODO handle this with a message to the user
    global_runing = false;
    break;
  case WM_ACTIVATEAPP:
    OutputDebugStringA("WM_ACTIVATEAPP\n");
    break;
  case WM_SYSKEYDOWN: // 普通键按下
  case WM_SYSKEYUP:   // 普通键释放
  case WM_KEYDOWN:    // 系统键按下
  case WM_KEYUP: {    // 系统键释放
    uint32 VKCode = wParam;
    boolean wasDown = (lParam & (1 << 30)) != 0; //  之前 按下|松开
    boolean isDown = (lParam & (1 << 31)) == 0;  // 当前 按下|松开

    if (wasDown == isDown) break;

    if (VKCode == 'W') {
    } else if (VKCode == 'A') {
    } else if (VKCode == 'S') {
    } else if (VKCode == 'D') {
    } else if (VKCode == 'Q') {
    } else if (VKCode == 'E') {
    } else if (VKCode == VK_UP) {
    } else if (VKCode == VK_LEFT) {
    } else if (VKCode == VK_DOWN) {
    } else if (VKCode == VK_RIGHT) {
    } else if (VKCode == VK_ESCAPE) {
      OutputDebugStringA("VK_ESCAPE: ");
      if (isDown)
        OutputDebugStringA("isDown ");
      if (wasDown)
        OutputDebugStringA("wasDown ");
      OutputDebugStringA("\n");
    } else if (VKCode == VK_SPACE) {
    } else {
    }

    int32 altKeyWasDown = lParam & (1 << 29); // 按住了ALT
    if (VKCode == VK_F4 && altKeyWasDown) global_runing = false;

    break;
  }
  case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC deviceContext = BeginPaint(window, &paint);
    auto [width, height] = getWindowDimension(window);
    win64DisplayBufferInWindow(&globalBackBuffer, deviceContext, width, height);
    EndPaint(window, &paint);
    break;
  }
  default:
    result = DefWindowProc(window, message, wParam, lParam);
    break;
  }

  return result;
}
