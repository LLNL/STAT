#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#include "CallpathRuntime.h"

extern "C" {

void callpath_wrap_init(int rank = -1);
void callpath_wrap_walk(const char *filename);

} //extern "C"
