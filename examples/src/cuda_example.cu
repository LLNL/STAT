#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef USEMPI
#include "mpi.h"
#endif

#define N (32*10)
#define THREADS_PER_BLOCK 32

__device__ void foo()
{
   int i, x, y;
#ifdef NOHANG
   for (i = 0; i <= 1000000; i++)
#else
   for (i = 0; i >=0; i++)
#endif
   {
    x = i;
    y = x + 1;
   }
}

__device__ void bar()
{
    foo();
}

__global__
void add(int *a, int *b) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i<N) {
        b[i] = 2*a[i];
    }
    if (threadIdx.x % 32 == 0)
        bar();
    else
        bar();
}

__global__
void add2(int *a, int *b) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i<N) {
        b[i] = 2*a[i];
    }
    bar();
}

int main(int argc, char **argv) {
    int ha[N], hb[N], hc[N];
#ifdef USEMPI
    int numtasks, rank;
    char hostname[256];
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    gethostname(hostname, 256);
    printf("Hello world from %s %d/%d\n", hostname, rank, numtasks);
#endif
    int *da, *db;
    cudaMalloc((void **)&da, N*sizeof(int));
    cudaMalloc((void **)&db, N*sizeof(int));
    int *dc, *dd;
    cudaMalloc((void **)&dc, N*sizeof(int));
    cudaMalloc((void **)&dd, N*sizeof(int));
    for (int i = 0; i<N; ++i)
        ha[i] = i;
    cudaMemcpy(da, ha, N*sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(dc, ha, N*sizeof(int), cudaMemcpyHostToDevice);
#ifdef USEMPI
    if (rank % 2 == 0)
#endif
    add<<<N/THREADS_PER_BLOCK, THREADS_PER_BLOCK>>>(da, db);
    add2<<<N/THREADS_PER_BLOCK, THREADS_PER_BLOCK>>>(dc, dd);
    cudaMemcpy(hc, dd, N*sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(hb, db, N*sizeof(int), cudaMemcpyDeviceToHost);
    for (int i = 0; i<N; ++i) {
      if (i == 99)
        printf("%d\n", hb[i], hc[i]);
    }
    cudaFree(da);
    cudaFree(db);
    return 0;
}
