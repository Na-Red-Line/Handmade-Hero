#pragma once

#include "handmade_platform.h"
union v2 {
  struct {
    f32 X;
    f32 Y;
  };
  f32 E[2];
};

inline v2 operator*(v2 A, f32 B) {
  v2 result = {};
  result.X = A.X * B;
  result.Y = A.Y * B;
  return result;
}

inline v2 operator*(f32 B, v2 A) {
  v2 result = A * B;
  return result;
}

inline v2 &operator*=(v2 &A, f32 B) {
  A = A * B;
  return A;
}

inline v2 operator+(v2 A, v2 B) {
  v2 result = {};
  result.X = A.X + B.X;
  result.Y = A.Y + B.Y;
  return result;
}

inline v2 &operator+=(v2 &A, v2 B) {
  A = A + B;
  return A;
}

inline v2 operator-(v2 A, v2 B) {
  v2 result = {};
  result.X = A.X - B.X;
  result.Y = A.Y - B.Y;
  return result;
}

inline v2 &operator-=(v2 &A, v2 B) {
  A = A - B;
  return A;
}

inline f32 square(f32 A) {
  f32 result = A * A;
  return result;
}

inline f32 inner(v2 A, v2 B) {
  f32 result = A.X * B.X + A.Y * B.Y;
  return result;
}
