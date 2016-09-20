#!/bin/env python

import subprocess
import sys
import argparse

rms = {"alps":"aprun", "slurm":"srun"}

def jobid_to_hostname_pid(rm, jobid, remoteshell):
    pids = []
    remotehost = None

    if rm == None or rm.lower() == 'auto':
        rm = auto_detect_rm()
    if rm == None:
        return remotehost, None, pids

    rm = rm.lower()
    if rm == 'slurm':
        proc = subprocess.Popen(["squeue", "-j", jobid, "-tr", "-o", '%B"'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ret = proc.stdout.read()
        lines = ret.splitlines()
        if not lines or lines[0].find("EXEC_HOST") != 0 or len(lines) < 2:
            return None, None, []
        if lines[1][-1] == '"':
            lines[1] = lines[1][:len(lines[1]) - 1]
        remotehost = lines[1]
    if rm == 'alps':
        proc = subprocess.Popen(["qstat", "-f", jobid], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ret = proc.stdout.read()
        lines = ret.splitlines()
        for line in lines:
            if line.find("login_node_id") != -1:
                remotehost = line.split(' = ')[1]
                break
    if remotehost != None:
        if remotehost.find('.localdomain') != -1:
            import socket
            domain_name =  socket.getfqdn().strip(socket.gethostname())
            remotehost = remotehost[:remotehost.find('.localdomain')]
        proc = subprocess.Popen([remoteshell, remotehost, "ps", "x"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ret = proc.stdout.read()
        lines = ret.splitlines()
        fmt = lines[0].split()
        pid_index = fmt.index('PID')
        for line in lines[1:]:
            if line.find(rms[rm]) != -1:
                pids.append(int(line.split()[pid_index]))
    return remotehost, rms[rm], pids

def auto_detect_rm():
    for rm_key in rms:
        ret = subprocess.call(["which", rms[rm_key]], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if ret == 0:
            rm = rm_key
            return rm
    return None

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Translate job ID to hostname:PID')
    parser.add_argument('-r', '--rm', dest='rm', default=None)
    parser.add_argument('-s', '--remoteshell', dest='remoteshell', default='rsh')
    parser.add_argument('jobid')
    args = parser.parse_args()
    rm = vars(args)['rm']
    remoteshell = vars(args)['remoteshell']
    jobid = vars(args)['jobid']

    if rm == None:
        rm = auto_detect_rm()

    hostname, rm_exe, pids = jobid_to_hostname_pid(rm, jobid, remoteshell)
    if hostname == None or pids == []:
        sys.stderr.write('failed to find hostname:pid for specified jobid\n')
        sys.exit(1)
    for pid in pids:
        print "%s:%d" %(hostname, pid)
