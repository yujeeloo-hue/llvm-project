#include <windows.h>

__declspec(noinline) int tick(int value) { return value + 1; }

int main() {
  volatile int value = 0;
  for (;;) {
    value = tick(value);
    Sleep(50);
  }
}
