#pragma once

#include <stdlib.h>
#include <time.h>

int random() {
  static bool initializer = false;
  if (!initializer) {
    srand((unsigned)time(nullptr));
    initializer = true;
  }
  return rand();
}
