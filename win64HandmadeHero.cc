#ifndef UNICODE
#define UNICODE
#endif

#include "inc.h"
#include <dsound.h>
#include <windows.h>
#include <xinput.h>

struct win64_offscreen_buffer {
  BITMAPINFO info;
  VOID *memory;
  int width;
  int height;
  int bytesPerPixel = 4;
  int pitch;
};

static bool global_runing;
static win64_offscreen_buffer globalBackBuffer;
static LPDIRECTSOUNDBUFFER globalDSoundBuffer;

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
  float sint;               // 余弦对应的周期
  int latencySampleCount;   // 声音延迟
};

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
static void win64RenderWeirGradient(win64_offscreen_buffer *buffer, int xOffset, int yOffset);
static void win64ResizeDIBSection(win64_offscreen_buffer *buffer, int width, int height);
static void win64DisplayBufferInWindow(win64_offscreen_buffer *buffer, HDC deviceContext, int windowWidth, int windowHeight);
// 填充音频缓冲区
static void win64FillSoundBuffer(win64_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite);

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

  win64_sound_output soundOutput = {
      0,
      48000,
      3000,
      256,
      soundOutput.samplesPerSecond / 256,
      sizeof(int16) * 2,
      soundOutput.samplesPerSecond * soundOutput.bytesPerSample,
      0.f,
      soundOutput.samplesPerSecond / 15,
  };
  win64LoadInitDSound(window, soundOutput.samplesPerSecond, soundOutput.DSoundBufferSize);
  // 初始化音频缓冲区并播放音频
  win64FillSoundBuffer(&soundOutput, 0, soundOutput.latencySampleCount * soundOutput.bytesPerSample);
  globalDSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

  // 图形
  int xOffset = 0;
  int yOffset = 0;

  global_runing = true;
  while (global_runing) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) global_runing = false;

      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    DWORD dwResult;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
      XINPUT_STATE state;
      ZeroMemory(&state, sizeof(XINPUT_STATE));

      // Simply get the state of the controller from XInput.
      dwResult = XInputGetState(i, &state);

      if (dwResult == ERROR_SUCCESS) {
        XINPUT_GAMEPAD *pad = &state.Gamepad;

        bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
        bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
        bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
        bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
        bool start = pad->wButtons & XINPUT_GAMEPAD_START;
        bool leftShoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
        bool rightShoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
        bool A = pad->wButtons & XINPUT_GAMEPAD_A;
        bool B = pad->wButtons & XINPUT_GAMEPAD_B;
        bool X = pad->wButtons & XINPUT_GAMEPAD_X;
        bool Y = pad->wButtons & XINPUT_GAMEPAD_Y;

        int16 stickX = pad->sThumbRX;
        int16 stickY = pad->sThumbRY;

        // TODO 处理控制器死区 XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
        xOffset += stickX / 4096;
        yOffset += stickY / 4096;

        if (A) yOffset += 2;

        soundOutput.toneHz = 256 + (int)(128.f * ((float)stickY / 30000.f));
        soundOutput.wavePeroid = soundOutput.samplesPerSecond / soundOutput.toneHz;

      } else {
        // NOTE The controller is not available
      }
    }

    // 手柄震动
    XINPUT_VIBRATION vibration = {60000, 60000};
    XInputSetState(0, &vibration);

    win64RenderWeirGradient(&globalBackBuffer, xOffset, yOffset);

    DWORD playCursor;  // 当前播放光标
    DWORD writeCursor; // 可写入光标
    if (SUCCEEDED(globalDSoundBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
      DWORD byteToLock = (soundOutput.runingSampleIndex * soundOutput.bytesPerSample) % soundOutput.DSoundBufferSize;
      DWORD targetCursor =
          (playCursor + soundOutput.latencySampleCount * soundOutput.bytesPerSample) % soundOutput.DSoundBufferSize;
      DWORD bytesToWrite =
          byteToLock > targetCursor ? soundOutput.DSoundBufferSize - byteToLock + targetCursor : targetCursor - byteToLock;

      win64FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite);
    }

    auto [width, height] = getWindowDimension(window);
    win64DisplayBufferInWindow(&globalBackBuffer, deviceContext, width, height);

    ++xOffset;
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

static void win64RenderWeirGradient(win64_offscreen_buffer *buffer, int xOffset, int yOffset) {
  int w = buffer->width;
  int h = buffer->height;

  uint8 *row = (uint8 *)buffer->memory;
  for (int y = 0; y < h; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = 0; x < w; ++x) {
      // memory order: RR GG BB xx
      // loaded in:    xx BB GG RR
      // window:       xx RR GG BB
      // memory order: BB GG RR xx
      uint8 blue = x + xOffset;
      uint8 green = y + yOffset;
      *pixel++ = ((green << 8) | blue);
    }
    row += buffer->pitch;
  }
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

static void win64FillSoundBuffer(win64_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite) {
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

  int16 *sampleOut = (int16 *)region1;
  DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
  for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
    // 频率改变导致相位错乱，产生噪音
    // float t = ((float)soundOutput->runingSampleIndex / (float)soundOutput->wavePeroid) * 2.0f * PI;
    float sineValue = sinf(soundOutput->sint);
    int16 sampleValue = (int16)(sineValue * soundOutput->toneVolume);
    // 写入左右双声道
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
    // 适应频率改变
    soundOutput->sint += (PI * 2.0f) / (float)soundOutput->wavePeroid;
    ++soundOutput->runingSampleIndex;
  }

  sampleOut = (int16 *)region2;
  DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
  for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
    float sineValue = sinf(soundOutput->sint);
    int16 sampleValue = (int16)(sineValue * soundOutput->toneVolume);
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
    soundOutput->sint += (PI * 2.0f) / (float)soundOutput->wavePeroid;
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

    bool32 altKeyWasDown = lParam & (1 << 29); // 按住了ALT
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
