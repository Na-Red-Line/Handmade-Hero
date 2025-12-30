#ifndef UNICODE
#define UNICODE
#endif

#include "inc.h"
#include <windows.h>

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

static win64_window_dimension getWindowDimension(HWND window);
static void win64RenderWeirGradient(win64_offscreen_buffer buffer, int xOffset, int yOffset);
static void win64ResizeDIBSection(win64_offscreen_buffer *buffer, int width, int height);
static void win64UpdateWindow(win64_offscreen_buffer buffer, HDC deviceContext, int windowWidth, int windowHeight);

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

    win64RenderWeirGradient(globalBackBuffer, xOffset, yOffset);

    HDC deviceContext = GetDC(window);
    auto dem = getWindowDimension(window);
    win64UpdateWindow(globalBackBuffer, deviceContext, dem.width, dem.height);
    ReleaseDC(window, deviceContext);

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

static void win64RenderWeirGradient(win64_offscreen_buffer buffer, int xOffset, int yOffset) {
  int w = buffer.width;
  int h = buffer.height;

  uint8 *row = (uint8 *)buffer.memory;
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
    row += buffer.pitch;
  }
}

static void win64UpdateWindow(win64_offscreen_buffer buffer,
                              HDC deviceContext,
                              int windowWidth, int windowHeight) {
  StretchDIBits(deviceContext,
                0, 0, windowWidth, windowHeight,
                0, 0, buffer.width, buffer.height,
                buffer.memory,
                &buffer.info,
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
  buffer->info.bmiHeader.biHeight = -height;
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
  case WM_PAINT: {
    PAINTSTRUCT paint;
    auto windowDimension = getWindowDimension(window);
    HDC deviceContext = BeginPaint(window, &paint);
    auto dem = getWindowDimension(window);
    win64UpdateWindow(globalBackBuffer, deviceContext, dem.width, dem.height);
    EndPaint(window, &paint);
    break;
  }
  default:
    result = DefWindowProc(window, message, wParam, lParam);
    break;
  }

  return result;
}
