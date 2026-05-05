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

inline v2 operator-(v2 A) {
  v2 result = {};
  result.X = -A.X;
  result.Y = -A.Y;
  return result;
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

inline f32 lengthSq(v2 A) {
  f32 result = inner(A, A);
  return result;
}

inline i32 signOf(i32 value) {
  i32 result = value >= 0 ? 1 : -1;
  return result;
}

struct Rectangle2
{
	v2 Min;
	v2 Max;
};

inline Rectangle2 rect_Min_Max(v2 Max, v2 Min)
{
	Rectangle2 result;

	result.Max = Max;
	result.Min = Min;

	return result;
}

inline Rectangle2 rect_center_half_dim(v2 center, v2 halfDim)
{
	Rectangle2 result;

	result.Max = center + halfDim;
	result.Min = center - halfDim;

	return result;
}

inline Rectangle2 rect_center_dim(v2 center, v2 dim)
{
	Rectangle2 result = rect_center_half_dim(center, 0.5f * dim);
	return result;
}

inline bool has_in_rectangle(Rectangle2 rectangle, v2 test)
{
	bool result = (test.X >= rectangle.Min.X &&
	               test.X <= rectangle.Max.X &&
	               test.Y >= rectangle.Min.Y &&
	               test.Y <= rectangle.Max.Y);

	return result;
}
