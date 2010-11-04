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

void work();
void do_stuff(int numtasks, int rank, int iter);
void do_SendOrStall(int to, int tag, int rank, int *buf, MPI_Request *req, int n, int iter);
void do_Receive(int from, int tag, int *buf, MPI_Request *req);
void compute();

char hostname[256];

int main (int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    work();
    MPI_Finalize();
    return 0;
}

void work()
{
    int i;
    //int numtasks, rank, i, iters = 2;
    int numtasks, rank, iters = 2;
    gethostname( hostname, 256);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    for (i = 0; i < iters; i++)
        do_stuff(numtasks, rank, i);
    do_stuff(numtasks, rank, iters + 1);
    MPI_Barrier(MPI_COMM_WORLD);
}

void do_stuff(int numtasks, int rank, int iter)
{
    int next, prev, buf[2], tag=2;
    MPI_Request reqs[2];
    MPI_Status stats[2];
    prev = rank-1;
    next = rank+1;
    if (rank == 0)  
        prev = numtasks - 1;
    if (rank == (numtasks - 1))  
        next = 0;
    do_Receive(prev, tag, &buf[0], &reqs[0]);
    do_SendOrStall(next, tag, rank, &buf[1], &reqs[1], numtasks, iter);
    MPI_Waitall(2, reqs, stats);
    if (rank == 1 && iter == 0)
    {
        fprintf(stderr, "MPI task %d stalling\n", rank);
        compute();
    }
}

void compute()
{
    while(1);
}

void do_SendOrStall(int to, int tag, int rank, int *buf, MPI_Request *req, int n, int iter)
{
    MPI_Isend(buf, 1, MPI_INT, to, tag, MPI_COMM_WORLD, req);
}

void do_Receive(int from, int tag, int *buf, MPI_Request *req)
{
    MPI_Irecv(buf, 1, MPI_INT, from, tag, MPI_COMM_WORLD, req);
}











































// end
