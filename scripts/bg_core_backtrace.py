"""@package bg_core_backtrace
An implementation for merging BlueGene lightweight core files into a .dot
format suitable for the Stack Trace Analysis Tool."""

__copyright__ = """Copyright (c) 2007-2014, Lawrence Livermore National Security, LLC."""
__license__ = """Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee <lee218@llnl.gov>, Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-624152.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
__author__ = ["Gregory Lee <lee218@llnl.gov>", "Dorian Arnold", "Matthew LeGendre", "Dong Ahn", "Bronis de Supinski", "Barton Miller", "Martin Schulz"]
__version__ = "2.1.0"

import sys
import os
from stat_merge_base import StatTrace, StatMerger, StatMergerArgs
from subprocess import Popen, PIPE

addr2line_exe = '/usr/bin/addr2line'
addr2line_map = {}
job_ids = []


class BgCoreTrace(StatTrace):
    def get_traces(self):
        global addr2line_map, addr2line_exe
        if self.options['addr2line'] != 'NULL':
            addr2line_exe = self.options['addr2line']
        self.rank = int(self.file_path[self.file_path.find('core.')+5:])
        f = open(self.file_path, 'r')
        line_number_traces = []
        function_only_traces = []
        in_stack = False
        job_id = -1
        for line in f:
            if line.find('Job ID') != -1:
                job_id = int(line.split(':')[1][1:])
                if job_id not in job_ids:
                    job_ids.append(job_id)
                    if len(job_ids) == 2 and self.options['jobid'] is None:
                        sys.stderr.write('\n\nWarning, multiple Job IDs detected in core files. Stack traces will still be merged into a single tree. To have traces differentiated by job ID, please use the -j or --jobid option\n\n')
                continue
            if line.find('+++STACK') != -1 or line.find('Function Call Chain') != -1:
                line_number_trace = []
                function_only_trace = []
                continue
            if line.find("Frame Address") == 0:
                in_stack = True
            if line.find('---STACK') != -1 or line.find('End of stack') != -1:
                if self.options['jobid'] is not None:
                    line_number_trace.insert(0, str(job_id))
                    function_only_trace.insert(0, str(job_id))
                line_number_traces.append(line_number_trace)
                function_only_traces.append(function_only_trace)
                line_number_trace = []
                function_only_trace = []
                in_stack = False
                continue
            line = line.strip(' ')
            if line.find('0x') == 0 or in_stack is True:
                addr = line.split(' ')[-1]
                if addr in addr2line_map:
                    line_info = addr2line_map[addr]
                else:
                    output = Popen([addr2line_exe, '-e', self.options["exe"], '--demangle', '-s', '-f', addr], stdout=PIPE).communicate()[0]
                    out_lines = output.split('\n')
                    line_info = '%s@%s' % (out_lines[0], out_lines[1])
                    line_info = line_info.replace('<', '\<').replace('>', '\>')
                    addr2line_map[addr] = line_info
                function = line_info[:line_info.find('@')]
                line_number_trace.insert(0, line_info)
                function_only_trace.insert(0, function)
        return [function_only_traces, line_number_traces]


class BgCoreMerger(StatMerger):
    def get_high_rank(self, trace_files):
        # determine the highest ranked task for graphlib initialization
        high_rank = 0
        for filename in trace_files:
            if filename.find('core.') != -1:
                rank = filename[filename.find('core.')+5:]
                rank = int(rank)
                if rank > high_rank:
                    high_rank = rank
        return high_rank


class BgCoreMergerArgs(StatMergerArgs):
    def __init__(self):
        StatMergerArgs.__init__(self)

        # add the -j --jobid option to prefix traces with job ID
        self.arg_map["jobid"] = self.StatMergerArgElement("j", False, None, None, "delineate traces based on Job ID in the core file")

        # add an agrument type to take the application executable path
        self.arg_map["exe"] = StatMergerArgs.StatMergerArgElement("x", True, str, "NULL", "the executable path")

        # add an agrument type to take the application executable path
        self.arg_map["addr2line"] = StatMergerArgs.StatMergerArgElement("a", True, str, "NULL", "the path to addr2line")

        # override the usage messages:
        self.usage_msg_synopsis = '\nThis tool will merge the stack traces from the user-specified lightweight core files and output 2 .dot files, one with just function names, the other with function names + line number information\n'
        self.usage_msg_command = '\nUSAGE:\n\tpython %s [options] -x <exe_path> -c <corefile>*\n\tpython tracer_merge.py [options] -x <exe_path> -c <core_files_dir>\n' % (sys.argv[0])
        self.usage_msg_examples = '\nEXAMPLES:\n\tpython %s -x a.out -c core.0 core.1\n\tpython %s -x a.out -c core.*\n\tpython %s -x a.out -c ./\n\tpython %s -x a.out -c core_dir\n' % (sys.argv[0], sys.argv[0], sys.argv[0], sys.argv[0])

    def is_valid_file(self, file_path):
        if file_path.find('core.') == 0:
            return True
        return False

    def error_check(self, options):
        if options["exe"] == "NULL":
            sys.stderr.write('\nYou must specify an executable with the -x or --exe= options\n')
            self.print_usage()
        if not os.path.exists(options["exe"]):
            sys.stderr.write('\nFailed to find executable "%s".\n' % options["exe"])
            self.print_usage()
        StatMergerArgs.error_check(self, options)


if __name__ == '__main__':
    merger = BgCoreMerger(BgCoreTrace, BgCoreMergerArgs)
    ret = merger.run()
    if ret != 0:
        sys.stderr.write('Merger failed\n')
        sys.exit(ret)
