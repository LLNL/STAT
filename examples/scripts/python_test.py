import logging
import threading
import time
has_mpi4py = True
try:
    from mpi4py import MPI
except:
    has_mpi4py = false

def thread_function(name):
    logging.info("Thread %s: starting", name)
    for i in range(120):
        time.sleep(5)
    logging.info("Thread %s: finishing", name)


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
    format = "%(asctime)s: %(message)s"
    logging.basicConfig(format=format, level=logging.INFO,
                        datefmt="%H:%M:%S")

    threads = list()
    for index in range(3):
        logging.info("Main    : create and start thread %d.", index)
        x = threading.Thread(target=thread_function, args=(index,))
        threads.append(x)
        x.start()

    for index, thread in enumerate(threads):
        logging.info("Main    : before joining thread %d.", index)
        thread.join()
        logging.info("Main    : thread %d done", index)
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
