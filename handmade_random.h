#pragma once

#include <stdlib.h>
#include <time.h>

inline int random() {
  static bool initializer = false;
  if (!initializer) {
    // srand((unsigned)time(nullptr));
    srand((unsigned)1);
    initializer = true;
  }
  return rand();
}
