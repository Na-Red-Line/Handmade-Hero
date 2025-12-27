#include <windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {
  return MessageBoxW(NULL, L"hello, world", L"caption", MB_OK | MB_ICONWARNING);
}
