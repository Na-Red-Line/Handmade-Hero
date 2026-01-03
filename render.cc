#include "inc.h"

void renderWeirGradient(game_offscreen_buffer buffer, int xOffset, int yOffset) {
  auto [memory, width, height, bytesPerPixel] = buffer;

  uint8 *row = (uint8 *)memory;
  int pitch = width * bytesPerPixel;

  for (int y = 0; y < height; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = 0; x < width; ++x) {
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
