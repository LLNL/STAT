/*
Copyright (c) 2007-2017, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee [lee218@llnl.gov], Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, Martin Schulz, Niklas Nielson, Nicklas Bo Jensen, Jesper Nielson, and Sven Karlsson.
LLNL-CODE-727016.
All rights reserved.

This file is part of STAT. For details, see http://www.github.com/LLNL/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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
void add(int *a, int *b)
{
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i<N)
        b[i] = 2*a[i];
    if (threadIdx.x % 32 == 0)
        bar();
    else
        bar();
}

__global__
void add2(int *a, int *b)
{
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i<N)
        b[i] = 2*a[i];
    bar();
}

int main(int argc, char **argv)
{
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
    for (int i = 0; i<N; ++i)
      if (i == 99)
        printf("%d\n", hb[i], hc[i]);
    cudaFree(da);
    cudaFree(db);
    return 0;
}
