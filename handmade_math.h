#pragma once

union v2 {
  struct {
    float X;
    float Y;
  };
  float E[2];
};

v2 operator*(v2 A, float B) {
  v2 result = {};
  result.X = A.X * B;
  result.Y = A.Y * B;
  return result;
}

v2 operator*(float B, v2 A) {
  v2 result = A * B;
  return result;
}

v2 &operator*=(v2 &A, float B) {
  A = A * B;
  return A;
}

v2 operator+(v2 A, v2 B) {
  v2 result = {};
  result.X = A.X + B.X;
  result.Y = A.Y + B.Y;
  return result;
}

v2 &operator+=(v2 &A, v2 B) {
  A = A + B;
  return A;
}

v2 operator-(v2 A, v2 B) {
  v2 result = {};
  result.X = A.X - B.X;
  result.Y = A.Y - B.Y;
  return result;
}

v2 &operator-=(v2 &A, v2 B) {
  A = A - B;
  return A;
}
