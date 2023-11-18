has_mpi4py = True
try:
    from mpi4py import MPI
except:
    has_mpi4py = false

def foo(rank):
    if rank == 0:
        bar(rank)
    else:
        foo(rank - 1)

def foo2(rank):
    if rank == 0:
        bar(rank)
    else:
        foo(rank - 1)

def bar(rank):
    import time
    for i in range(120):
        time.sleep(5)
    print('done sleeping, importing parser')
    import parser
    print ('importing socket')
    import socket
    print ('sleeping')
    time.sleep(15)

rank = 0
if has_mpi4py == True:
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()
if rank == 0:
    print('job starting')
if rank % 2 == 0:
    foo(rank)
else:
    foo2(rank)
if rank == 0:
    print ('done')
