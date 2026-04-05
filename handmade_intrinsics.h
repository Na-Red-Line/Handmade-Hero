#pragma once

#include "handmade_platform.h"

#include <math.h>

inline i32 roundFloatToI32(f32 value) {
  i32 result = (i32)roundf(value);
  return result;
}

inline i32 roundFloatToU32(f32 value) {
  u32 result = (u32)roundf(value);
  return result;
}

inline i32 floorFloatToI32(f32 value) {
  i32 result = (i32)floorf(value);
  return result;
}

inline i32 truncateFloatToI32(f32 value) {
  i32 result = (i32)value;
  return result;
}

inline f32 sin(f32 angle) {
  f32 result = sinf(angle);
  return result;
}

inline f32 cos(f32 angle) {
  f32 result = cosf(angle);
  return result;
}

inline f32 atan2(f32 Y, f32 X) {
  f32 result = atan2f(Y, X);
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

inline u32 rotateLeft(u32 value, i32 amount) {
#if COMPILER_MSVC
  u32 result = _rotl(value, amount);
#else
  amount &= 31;
  u32 result = (value << amount) | (value >> (32 - amount));
#endif

  return result;
}

inline u32 rotateRight(u32 value, i32 amount) {
#if COMPILER_MSVC
  u32 result = _rotr(value, amount);
#else
  amount &= 31;
  u32 result = (value >> amount) | (value << (32 - amount));
#endif

  return result;
}
