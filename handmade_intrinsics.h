#pragma once

#include "handmade_platform.h"

#include <math.h>

inline int32 roundFloatToInt32(float f32) {
  int32 result = (int32)roundf(f32);
  return result;
}

inline int32 roundFloatToUInt32(float f32) {
  uint32 result = (uint32)roundf(f32);
  return result;
}

inline int32 floorFloatToInt32(float f32) {
  int32 result = (int32)floorf(f32);
  return result;
}

inline int32 truncateFloatToInt32(float f32) {
  int32 result = (int32)f32;
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
