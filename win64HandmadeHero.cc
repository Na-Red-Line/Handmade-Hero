#ifndef UNICODE
#define UNICODE
#endif

#include "inc.h"
#include <windows.h>

// TODO this is global for now
static bool runing;
static BITMAPINFO bitmapInfo;
static VOID *bitmapMemory;
static int bitmapWidth;
static int bitmapHeight;

inline constexpr int bytesPerPixel = 4;

static void win64RenderWeirGradient(int xOffset, int yOffset) {
  int w = bitmapWidth;
  int h = bitmapHeight;

  uint8 *row = (uint8 *)bitmapMemory;
  int pitch = bytesPerPixel * w;
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
    row += pitch;
  }
}

static void win64ResizeDIBSection(int w, int h) {

  // TODO bulletproof this.
  // maybe don't free first, free after, then free first if that fails.

  if (bitmapMemory) {
    VirtualFree(bitmapMemory, 0, MEM_RELEASE);
  }

  bitmapWidth = w;
  bitmapHeight = h;

  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = bitmapWidth;
  bitmapInfo.bmiHeader.biHeight = -bitmapHeight;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;

  int bitmapMemorySize = (bitmapWidth * bitmapHeight) * bytesPerPixel;
  bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

static void win64UpdateWindow(HDC deviceContext, RECT *window) {
  int windowWidth = window->right - window->left;
  int windowHeight = window->bottom - window->top;

  StretchDIBits(deviceContext, 0, 0, bitmapWidth, bitmapHeight, 0, 0, windowWidth, windowHeight,
                bitmapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, PWSTR pCmdLine, int showCode) {
  WNDCLASS wc = {};

  wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW; // TODO need?
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = L"HandmadeHero";

  RegisterClass(&wc);
  HWND window = CreateWindowExW(0,                   // Optional window styles.
                                wc.lpszClassName,    // Window class
                                L"手工英雄",         // Window text
                                WS_OVERLAPPEDWINDOW, // Window style

                                // Size and position
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

                                NULL,     // Parent window
                                NULL,     // Menu
                                instance, // Instance handle
                                NULL      // Additional application data
  );

  if (window == NULL) {
    return 0;
  }

  ShowWindow(window, showCode); // TODO why need

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

    win64RenderWeirGradient(xOffset, yOffset);

    HDC deviceContext = GetDC(window);
    RECT clientRect;
    GetClientRect(window, &clientRect);
    win64UpdateWindow(deviceContext, &clientRect);
    ReleaseDC(window, deviceContext);

    ++xOffset;
    yOffset += 2;
  }

  return 0;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;

  switch (message) {
  case WM_SIZE: {
    RECT clientRect;
    GetClientRect(window, &clientRect);
    int w = clientRect.right - clientRect.left;
    int h = clientRect.bottom - clientRect.top;
    win64ResizeDIBSection(w, h);
    break;
  }
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
    HDC deviceContext = BeginPaint(window, &paint);
    RECT clientRect;
    GetClientRect(window, &clientRect);
    win64UpdateWindow(deviceContext, &clientRect);
    EndPaint(window, &paint);
    break;
  }
  default:
    result = DefWindowProc(window, message, wParam, lParam);
    break;
  }

  return result;
}
