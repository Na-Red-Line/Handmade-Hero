#pragma once

#include "handmade_platform.h"

#include <math.h>

inline i32 roundFloatToI32(float f32) {
  i32 result = (i32)roundf(f32);
  return result;
}

inline i32 roundFloatToU32(float f32) {
  u32 result = (u32)roundf(f32);
  return result;
}

inline i32 floorFloatToI32(float f32) {
  i32 result = (i32)floorf(f32);
  return result;
}

inline i32 truncateFloatToI32(float f32) {
  i32 result = (i32)f32;
  return result;
}

inline float sin(float angle) {
  float result = sinf(angle);
  return result;
}

inline float cos(float angle) {
  float result = cosf(angle);
  return result;
}

inline float atan2(float Y, float X) {
  float result = atan2f(Y, X);
  return result;
}

struct bit_scan_result {
  bool found;
  u32 index;
};

inline bit_scan_result findLeastSignificantSetBit(u32 value) {
  bit_scan_result result = {};

#if COMPILER_MSVC
  result.found = _BitScanForward((unsigned long *)&result.index, value);
#else
  for (u32 i = 0; i < 32; ++i) {
    if (value & (1 << i)) {
      result.index = i;
      result.found = true;
      break;
    }
  }
#endif

  return result;
}
