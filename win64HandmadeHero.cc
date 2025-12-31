#ifndef UNICODE
#define UNICODE
#endif

#include "inc.h"
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

static bool runing;
static win64_offscreen_buffer globalBackBuffer;

struct win64_window_dimension {
  int width;
  int height;
};

//
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

static void win64LoadXInput() {
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
  if (!XInputLibrary) XInputLibrary = LoadLibraryA("xinput1_3.dll");

  if (XInputLibrary) {
    XInputGetState_ = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState_ = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

static win64_window_dimension
getWindowDimension(HWND window);
static void win64RenderWeirGradient(win64_offscreen_buffer *buffer, int xOffset, int yOffset);
static void win64ResizeDIBSection(win64_offscreen_buffer *buffer, int width, int height);
static void win64UpdateWindow(win64_offscreen_buffer *buffer, HDC deviceContext, int windowWidth, int windowHeight);

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

  int xOffset = 0;
  int yOffset = 0;

  runing = true;
  while (runing) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) runing = false;

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

        if (A) yOffset += 2;

      } else {
        // NOTE The controller is not available
      }
    }

    XINPUT_VIBRATION vibration = {60000, 60000};
    XInputSetState(0, &vibration);

    win64RenderWeirGradient(&globalBackBuffer, xOffset, yOffset);

    auto [width, height] = getWindowDimension(window);
    win64UpdateWindow(&globalBackBuffer, deviceContext, width, height);

    ++xOffset;
    yOffset += 2;
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

static void win64UpdateWindow(win64_offscreen_buffer *buffer,
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
  buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;

  switch (message) {
  case WM_SIZE:
    break;
  case WM_DESTROY:
    // TODO handle this as an error - recreate window
    runing = false;
    break;
  case WM_CLOSE:
    // TODO handle this with a message to the user
    runing = false;
    break;
  case WM_ACTIVATEAPP:
    OutputDebugStringA("WM_ACTIVATEAPP\n");
    break;
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP: {
    uint32 VKCode = wParam;
    boolean wasDown = (lParam & (1 << 30)) != 0;
    boolean isDown = (lParam & (1 << 31)) == 0;

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
    break;
  }
  case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC deviceContext = BeginPaint(window, &paint);
    auto [width, height] = getWindowDimension(window);
    win64UpdateWindow(&globalBackBuffer, deviceContext, width, height);
    EndPaint(window, &paint);
    break;
  }
  default:
    result = DefWindowProc(window, message, wParam, lParam);
    break;
  }

  return result;
}
