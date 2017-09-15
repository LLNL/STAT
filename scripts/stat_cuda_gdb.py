import sys
import os
import logging
import pwd

try:
    from cuda_gdb import CudaGdbDriver
except Exception as e:
    print e
gdb_instances = {}

def new_gdb_instance(pid):

    # if TMPDIR is not set, you may get this error and fail to see GPU devices:
    #    The CUDA driver could not allocate operating system resources for attaching to the application.
    #
    #    An error occurred while in a function called from GDB.
    #    Evaluation of the expression containing the function
    #    (cudbgApiAttach) will be abandoned.
    #    When the function is done executing, GDB will silently stop.
    if "TMPDIR" not in os.environ:
        os.environ["TMPDIR"]="/var/tmp/%s" %(pwd.getpwuid( os.getuid() ).pw_name)

    gdb = CudaGdbDriver(pid, 'debug', 'log.txt')
    if gdb.launch() is False:
        return -1
    gdb_instances[pid] = gdb
    return pid

def attach(pid):
    gdb_instances[pid].attach()
    return 0

def detach(pid):
    gdb_instances[pid].detach()
    return 0

def pause(pid):
    gdb_instances[pid].pause()
    return 0

def resume(pid):
    gdb_instances[pid].resume()
    return 0

def get_all_host_traces(pid):
    ret = ''
    tids = gdb_instances[pid].get_thread_list()
    tids.sort()
    for thread_id in tids:
        bt = gdb_instances[pid].bt(thread_id)
        bt.reverse()
        for frame in bt:
            ret += '%s@%s:%d\n' %(frame['function'], frame['source'], frame['linenum'])
        ret += '#endtrace\n'
    return ret

def get_all_device_traces(pid, retries=5, retry_frequency=1000):
    ret = ''
    threads = gdb_instances[pid].get_cuda_threads(retries, retry_frequency)
    for thread in threads:
        gdb_instances[pid].cuda_block_thread_focus(thread['start_block'], thread['start_thread'])
        bt = gdb_instances[pid].cuda_bt()
        bt.reverse()
        ret += '#count#%d\n' %(thread['count'])
        for frame in bt:
            ret += '%s@%s:%d\n' %(frame['function'], frame['source'], frame['linenum'])
        ret += '#endtrace\n'
    return ret

if __name__ == "__main__":
    pids = sys.argv[1:]
    for pid in pids:
        pid = int(pid)
        handle = new_gdb_instance(pid)
        if handle == -1:
            sys.stderr.write('gdb launch failed\n')
            sys.exit(1)
        if attach(handle) != 0:
            sys.stderr.write('gdb attach failed\n')
            sys.exit(1)
    for pid in pids:
        pid = int(pid)
        host_traces = get_all_host_traces(pid)
        resume(pid)
        pause(pid)
        device_traces = get_all_device_traces(pid)
    #    devices = gdb_instances[pid].get_cuda_devices()
    #    print '\ndevices'
    #    for device in devices:
    #        if gdb_instances[pid].cuda_device_focus(device):
    #            print 'device focused', device
    #    kernels = gdb_instances[pid].get_cuda_kernels()
    #    print '\nkernels'
    #    for kernel in kernels:
    #        if gdb_instances[pid].cuda_kernel_focus(kernel):
    #            print 'kernel focused', kernel
    #    bt = gdb_instances[pid].cuda_bt()
    #    for frame in bt:
    #        print frame
    #    print
    #    gdb_instances[pid].kill()
    for pid in pids:
        pid = int(pid)
        print '\ndetaching %d' %(pid)
        detach(pid)
