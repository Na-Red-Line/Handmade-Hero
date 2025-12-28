#ifndef UNICODE
#define UNICODE
#endif

#include <stdbool.h>
#include <windows.h>

// TODO this is global for now
static bool Runing;
static BITMAPINFO bitmapInfo;
static VOID *bitmapMemory;
static HBITMAP bitmapHandle;
static HDC bitmapDeviceContext;

static void win64ResizeDIBSection(int w, int h) {

  // TODO bulletproof this.
  // maybe don't free first, free after, then free first if that fails.

  if (bitmapHandle != NULL) {
    DeleteObject(bitmapHandle);
  }

  if (!bitmapDeviceContext) {
    // TODO should we recreate these under cretain special circumstance
    bitmapDeviceContext = CreateCompatibleDC(0);
  }

  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = w;
  bitmapInfo.bmiHeader.biHeight = h;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;

  bitmapHandle =
      CreateDIBSection(bitmapDeviceContext, &bitmapInfo, DIB_RGB_COLORS, &bitmapMemory, 0, 0);
}

static void win64UpdateWindow(HDC deviceContext, int x, int y, int w, int h) {
  StretchDIBits(deviceContext, x, y, w, h, x, y, w, h, bitmapMemory, &bitmapInfo, DIB_RGB_COLORS,
                SRCCOPY);
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

  MSG msg;
  Runing = true;
  while (Runing) {
    BOOL messageResult = GetMessageA(&msg, 0, 0, 0);
    if (messageResult > 0) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    } else {
      break;
    }
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
    Runing = false;
    break;
  case WM_CLOSE:
    // TODO handle this with a message to the user
    Runing = false;
    break;
  case WM_ACTIVATEAPP:
    OutputDebugStringA("WM_ACTIVATEAPP\n");
    break;
  case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC deviceContext = BeginPaint(window, &paint);
    int x = paint.rcPaint.left;
    int y = paint.rcPaint.top;
    int w = paint.rcPaint.right - paint.rcPaint.left;
    int h = paint.rcPaint.bottom - paint.rcPaint.top;
    win64UpdateWindow(deviceContext, x, y, w, h);
    EndPaint(window, &paint);
    break;
  }
  default:
    result = DefWindowProc(window, message, wParam, lParam);
    break;
  }

  return result;
}
