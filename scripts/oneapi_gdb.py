#!/bin/env python

"""@package STATview
Visualizes dot graphs outputted by STAT."""

__copyright__ = """Modifications Copyright (C) 2022 Intel Corporation
SPDX-License-Identifier: BSD-3-Clause"""
__license__ = """Produced by Intel Corporation for Lawrence Livermore National Security, LLC.
Written by M. Oguzhan Karakaya oguzhan.karakaya@intel.com
LLNL-CODE-750488.
All rights reserved.

This file is part of STAT. For details, see http://www.github.com/LLNL/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
__author__ = ["M. Oguzhan Karakaya <oguzhan.karakaya@intel.com>"]
__version_major__ = 4
__version_minor__ = 1
__version_revision__ = 2
__version__ = "%d.%d.%d" %(__version_major__, __version_minor__, __version_revision__)

import sys
import os
import logging
import re
from gdb import GdbDriver, check_lines

def expand_simd_spec(simd):
    """
    Converts a merged simd specifier into a list of individual simd lanes.
    `simd` string is expected to be either an individual lane number
    or a comma-separated range list inside square brackets.
    """
    if simd[0] == '[':
        simd = simd[1:]
    lanes = []
    for item1 in simd.split(" "):
        for item2 in item1.split(","):
            if item2.isnumeric():
                lanes += item2
            else:
                pair = item2.split("-")
                for num in range(int(pair[0]), int(pair[1])+1):
                    lanes += str(num)
    return lanes

def parse_thread_info_line(line, parse_simd_lanes = False):
    """
    Extracts thread IDs from a `thread info` output produced by
    Intel(R) Distribution for GDB*. See oneapi_gdb_test.py for
    sample inputs and outputs.
    """
    tid_single_simd = re.compile(
        r"^[\s|*]*(\d+(\.?\d+)?)(?:(?::(\d+))?)\s*(?:Thread|LWP)")
    tid_multiple_simd = re.compile(
        r"^[\s|*]*(\d+(\.?\d+)?)(?:(?::\[(.*?)\])?)\s*(?:Thread|LWP)")
    match = re.search(tid_multiple_simd, line)
    if not match:
        return []
    if not parse_simd_lanes:
        return [ match[1] ]
    if not match[3]:
        match = re.search(tid_single_simd, line)
        if not match[3]:
            return [ match[1] ]
        return [ match[1] + ":" + match[3]]
    return [ match[1] + ":" + x for x in expand_simd_spec(match[3]) ]

def clean_cpp_template_brackets_and_call_signature(string):
    """
    Prettify function name output by Intel(R) Distribution for GDB*.
    Long template parameters and function signatures are collapsed.
    """
    sub_strings = []
    (found, begin, end, counter) = (False, 0, 0, 0)
    for idx, char in enumerate(string):
        if char == "<" and not found:
            found = True
            begin = idx
            counter = 1
        elif char == "<" and found:
            counter += 1
        elif char == ">" and found and counter > 1:
            counter -= 1
        elif char == ">" and found and counter == 1:
            end = idx
            counter -= 1
            found = False
            sub_strings.append(string[begin:end+1:])
    # Removing template variables from the function name
    for sub_string in sub_strings:
        string = string.replace(sub_string, "<...>")
    # Removing call signature for the function name
    ptrn = re.compile(r"\([^()]+\)(?=[^()]*$)")
    match = ptrn.findall(string)
    if match:
        string = string.replace(match[-1], "(...)")

    return string

def parse_frameinfo_from_backtrace(string):
    """
    Takes the gdb backtrace output and call
    clean_cpp_template_brackets_and_call_signature function over
    extracted function names. See oneapi_gdb_test.py for
    sample inputs and outputs.
    """
    function = 'Unknown'
    source = '??'
    linenum = 0
    error = True

    frame_fromat = re.compile(
        r"^#\d+\s+(?:0x\S+)*(?: in )*(.*)\s\(.*\)\s(?:at|from)\s([^:]+)(?::)?(\d+)?$")
    match = frame_fromat.match(string)
    if match:
        error = False
        function, source, linenum = match.groups()
        if linenum is None:
            linenum = 0
        else:
            linenum = int(linenum)
    function = clean_cpp_template_brackets_and_call_signature(function)
    return {'function':function,
            'source':source,
            'linenum':linenum,
            'error':error}

class OneAPIGdbDriver(GdbDriver):
    """ A class to drive Intel(R) Distribution for GDB* """

    gdb_command = 'gdb-oneapi'
    parse_simd_lanes = False
    if 'STAT_COLLECT_SIMD_BT' in os.environ and \
        os.environ['STAT_COLLECT_SIMD_BT'] == "1":
        parse_simd_lanes = True

    def get_thread_list(self):
        """
        Gets the list of threads in the target process. For
        Intel(R) Distribution for GDB* this function extracts SIMD
        lane information as part of a thread ID.
        It is to improve the resulting representation by adding the
        information on number of active SIMD lanes along with stack
        trace information.
        """
        logging.info('gdb-oneapi: info threads')
        tids = []
        lines = self.communicate("info threads")
        logging.debug('%s', repr(lines))
        for line in lines:
            if "inactive" in line:
                continue
            tids += parse_thread_info_line(line, self.parse_simd_lanes)
        return tids

    def bt(self, thread_id):
        """
        Gets a backtrace from the requested thread id.
        returns list of frames, where each frame is a map of attributes.
        """
        logging.info('GDB thread bt ID %s' %(thread_id))
        ret = []

        lines = self.communicate("thread apply %s bt" %thread_id)

        lines = lines[2:]
        for line_num, line in enumerate(lines):
            if line[0] != '#':
                continue
            logging.debug('Parsing line #%d: %d', line_num, line)
            ret.append(parse_frameinfo_from_backtrace(line))
        return ret

    def attach(self):
        """
        Attaches to the target process and checkes if there is
        an error related to gdbserver-gt at the gdb console output.
        """
        logging.info('GDB attach to PID %d' %(self.pid))
        lines = self.communicate("attach %d" %self.pid)
        # We need to check if there is an error with starting
        # gdbserver-gt and inform the user for possible missing
        # device backtrace features.
        ptrn = re.compile(
            r"^intelgt\:\s*(\S*)\s*failed to start\.")
        for line in lines:
            match = ptrn.match(line)
            if match:
                gdbserver_name = match.groups()[0]
                logging.error("""
OneAPI %s initialization failed for process %d. Device backtrace will not be available.""",
                    gdbserver_name, self.pid)
        logging.debug('%s', repr(lines))
        return check_lines(lines)

    def target_core(self, path):
        """
        Intel(R) Distribution for GDB* targets a core dump
        file given by path.
        """
        logging.info('GDB open core %s' %(path))
        self.communicate("target core %s" %path)

if __name__ == "__main__":
    gdb = OneAPIGdbDriver(0, 'debug', 'log.txt')
    if gdb.launch() is False:
        sys.stderr.write('gdb launch failed\n')
        sys.exit(1)
    gdb.target_core(sys.argv[1])
    thread_ids = gdb.get_thread_list()
    thread_ids.sort()
    for tid in thread_ids:
        bt = gdb.bt(tid)
        print("Thread", tid)
        for i, frame in enumerate(bt):
            print("%d) %s:%s" %(i, frame['source'], frame['linenum']))
        print()
    gdb.close()
