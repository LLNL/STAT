#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

extern "C" {
#include "depcore.h"

int globalMpiRank;
void depositcorewrap_init();
void depositcore_wrap_signal_handler(int signum);

void depositcorewrap_init()
{
    //fprintf(stderr, "init depositcore wrapper\n");
    signal(SIGUSR1, depositcore_wrap_signal_handler);
}

void depositcore_wrap_signal_handler(int signum)
{
    //fprintf(stderr, "MPI rank %d caught signal %d\n", globalMpiRank, signum);
    LDC_Depcore_cont(globalMpiRank, 1);
}

} //extern "C"
