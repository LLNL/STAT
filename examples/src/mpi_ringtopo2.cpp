/******************************************************************************
* FILE: mpi_ringtopo.c
* DESCRIPTION:
*   MPI tutorial example code: Non-Blocking Send/Receive
* AUTHOR: Blaise Barney
* LAST REVISED: 04/02/05
******************************************************************************/
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

void do_SendOrStall(int to, int tag, int rank, int* buf, MPI_Request* req, int n);
void do_Receive(int from, int tag, int* buf, MPI_Request* req);

char hostname[256];
int sleeptime = 0;

struct test_t {
  int a;
  long b;
  void* c;
};

int main (int argc, char *argv[])
{
    int rank, i;
    int numtasks, next, prev, buf[2], tag=2;
    MPI_Request reqs[2];
    MPI_Status stats[2];

    int timeout = 3;
    sleep(timeout);

    MPI_Init(&argc,&argv);

    if (argc > 1)
        sleeptime = atoi(argv[1]);
    gethostname( hostname, 256);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    prev = rank-1;
    next = rank+1;
    if (rank == 0)  
        prev = numtasks - 1;
    if (rank == (numtasks - 1))  
        next = 0;

    if(sleeptime < 0 && rank == 6) raise(SIGSEGV); 
  for (i = 0; i < 2; i++) {
    do_Receive(prev, tag, &buf[0], &reqs[0]);

    do_SendOrStall(next, tag, rank, &buf[1], &reqs[1], numtasks);
    MPI_Waitall(2, reqs, stats);

    MPI_Barrier(MPI_COMM_WORLD);
  }
    MPI_Finalize();
    sleep(5);
    if (rank == 0)
      printf("mpi_ringtopo Done\n");
    return 0;
}

void do_SendOrStall(int to, int tag, int rank, int* buf, MPI_Request* req, int n)
{
    int i = 0;
    float val = rank * 1.1; double val2 = rank * 2.2; long val3 = rank * 2; int *ip = &i;
    struct test_t st;
    st.a = rank;
    st.b = to % 4;
    st.c = req;

    if (rank == 1)
    {
        if (sleeptime == 0)
        {
            printf("%s, MPI task %d of %d stalling\n", hostname, rank, n);
            fflush(stdout);
            while(1) ;
        }
        else
        {
            for (i = sleeptime/10; i > 0; i = i - 1)
            {
                printf("%s, MPI task %d of %d stalling for %d of %d seconds\n", hostname, rank, n, i*10, sleeptime);
                fflush(stdout);
                sleep(10);
            }
            printf("%s, MPI task %d of %d proceeding\n", hostname, rank, n);
            fflush(stdout);
        }
    }    

    MPI_Isend(buf, 1, MPI_INT, to, tag, MPI_COMM_WORLD, req);
}

void do_Receive(int from, int tag, int* buf, MPI_Request* req)
{
    MPI_Irecv(buf, 1, MPI_INT, from, tag, MPI_COMM_WORLD, req);
}






























































//end
