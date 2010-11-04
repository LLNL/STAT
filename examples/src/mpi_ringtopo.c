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

void do_SendOrStall(int to, int tag, int rank, int* buf, MPI_Request* req, int n);
void do_Receive(int from, int tag, int* buf, MPI_Request* req);

char hostname[256];

int main (int argc, char *argv[])
{
    int numtasks, rank, next, prev, buf[2], tag=2;
    MPI_Request reqs[2];
    MPI_Status stats[2];

    gethostname( hostname, 256);
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    prev = rank-1;
    next = rank+1;
    if (rank == 0)  
        prev = numtasks - 1;
    if (rank == (numtasks - 1))  
        next = 0;

    do_Receive(prev, tag, &buf[0], &reqs[0]);
    do_SendOrStall(next, tag, rank, &buf[1], &reqs[1], numtasks);
    MPI_Waitall(2, reqs, stats);

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}

void do_SendOrStall(int to, int tag, int rank, int* buf, MPI_Request* req, int n)
{
    if (rank == 1)
    {
    	fprintf(stderr, "MPI task 1 of %d stalling\n", n);
        while(1) ;
    }	

    MPI_Isend(buf, 1, MPI_INT, to, tag, MPI_COMM_WORLD, req);
}

void do_Receive(int from, int tag, int* buf, MPI_Request* req)
{
    MPI_Irecv(buf, 1, MPI_INT, from, tag, MPI_COMM_WORLD, req);
}






























































//end
