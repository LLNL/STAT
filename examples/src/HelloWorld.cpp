#include <stdio.h>
#include <stdlib.h>

extern "C" {
  extern int runtime_dummy(void);
  extern void frontend_setup();
}

void frontend_setup() {
  int result = (runtime_dummy() == 127);

  printf("\nJitted HelloWorld\n");
  printf("Run-time call-back: %s\n", result ? "OK" : "FAILURE!");
}
