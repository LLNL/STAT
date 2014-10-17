"""@package coredump
An implementation for merging core files into a .dot
format suitable for the Stack Trace Analysis Tool.

Adapted from a script initially written by Dane Gardner 
"""

"""
TODO: There are still issues with gdb subprocesses remaining after the 
      script has exited; seems to be only when the user sends an 
      interrupt
"""

__copyright__ = """Copyright (c) 2007-2014, Lawrence Livermore National Security, LLC."""
__license__ = """Produced at the Lawrence Livermore National Laboratory
Written by Dane Gardner, Gregory Lee <lee218@llnl.gov>, Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-624152.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
__author__ = ["Dane Gardner", "Gregory Lee <lee218@llnl.gov>", "Dorian Arnold", "Matthew LeGendre", "Dong Ahn", "Bronis de Supinski", "Barton Miller", "Martin Schulz"]
__version__ = "2.1.0"

###############################################################################
import signal, os, sys

try:
    from stat_merge_base import StatTrace, StatMerger, StatMergerArgs
except:
    sys.stderr.write("The following required library is missing: stat_merge_base\n")
    sys.exit(1)

try:
    from bg_core_backtrace import BgCoreTrace, BgCoreMerger, BgCoreMergerArgs
except:
    sys.stderr.write("The following required library is missing: stat_merge_base\n")
    sys.exit(1)

import subprocess, re, threading, glob, logging
from optparse import OptionParser
from datetime import datetime

class Gdb(object):
    def __init__ (self, options):
        """Constructor"""

        #Set up the options variables
        self.directory =  options["directory"]
        self.coredir =    (options["coredir"])    and options["coredir"]    or self.directory
        self.exedir =     (options["exedir"])     and options["exedir"]     or self.directory
        self.sourcepath = (options["sourcepath"]) and options["sourcepath"] or ''
        self.objectpath = (options["objectpath"]) and options["objectpath"] or ''
        self.executable = None
        self.corefile = None
        self.subprocess = None

    def open(self, corefile, executable):
        """Opens the gdb subprocess using the given arguments
           \param corefile the corefile to open
           \param executable the executable file to use, may be empty"""
  
        args = []
        args.append('gdb')
        args.append('-ex')
        args.append("set pagination 0")
        args.append('-ex')
        args.append("cd %s" %(self.directory))
        args.append('-ex')
        args.append("path %s" %(self.objectpath))
        args.append('-ex')
        args.append("directory %s" %(self.sourcepath))
        if corefile:
            self.corefile = corefile
            args.append("--core=%s/%s" %(self.coredir, self.corefile))
        if executable:
            self.executable = executable
            args.append("%s/%s" %(self.exedir, self.executable))
        self.subprocess = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return self.readlines()

    def close(self):
        """Closes the gdb subprocess"""
        try:
            try:
                self.subprocess.communicate("quit\n")
                self.subprocess = None
            except: pass
        finally:
            self.kill()   #Ensure closure
  
    def kill(self):
        try:
            if self.subprocess:  #Kill the child process
                logging.warn("Killing gdb subprocess")
                os.kill(self.subprocess.pid, signal.SIGKILL)
                self.subprocess = None
        except OSError: pass
  
  
    def __del__(self):
        """Destructor"""
        self.close()
  
  
    def readlines(self):
        """Simply reads the lines, until we get to the '(gdb)' prompt
           prevents blocking from a lack of EOF with a stdout.readline() loop"""
        line = ''
        lines = []
        while True:
            ch = self.subprocess.stdout.read(1)
            if ch == '\n':
                lines.append(line)
                ch = ''
                line = ''
            elif ch == ' ':    #Check for the prompt, and return if we get it
                if '(gdb)' in line:
                    break
            line += ch
        return lines
  
  
    def communicate(self, command):
        """Sends the command to gdb, and returns a list of outputted lines
           \param command the command to send to gdb
           \returns a list of lines from a merged stdout/stderr stream"""
        if not command.endswith('\n'):
            command += '\n'
        self.subprocess.stdin.write(command)
        return self.readlines()

###############################################################################
next_rank = 0
rank_list = []
class CoreFile:
    """RegEx for finding information about a frame from a back trace.
         group(1) = value of the frame number
         group(2) = name of the function
         group(3) = source file and line number in format "file:line"  --this value may be None"""
    __reFrame = re.compile(r"#(\d+)\s+(?:0x\S+\s+in\s+)?(\S+)\s+\(.*\)(?:\s+at\s+(\S+:\d+))?")
    __options = None

    def __init__ (self, coreFile, options):
        if CoreFile.__options is None and not options is None:
            CoreFile.__options = options
        self.coreData = {'coreFile': coreFile,
                         'rank': next_rank,
                         'rankSize': None,
                         'traces': []}

    def addFunctions(self, functions):
        """If the functions array isn't empty, reverse it to get a proper trace, and
           append it to merge
           \param functions a list of strings, each containing a function name"""
    
        if len(functions):    #If we've got a trace from the last one, add it to the graph
            logging.debug("Appending trace")
            trace = []
            while len(functions):    #Reverse the trace
                trace.append(functions.pop())
            self.coreData['traces'].append(trace)
  
    def getFunctionValue(self, gdb, funcName, varPosition):
        """Run through the source and the stack frames to find a value for the
           requested variable, if possible (i.e. function was called).
           \param gdb the open gdb subprocess
           \paran funcName string with the desired function call name
           \param varPosition int with desired variable position
           \warning Ensure that symbols are loaded before calling this function"""
    
        reThread = re.compile("\* (\d) Thread")
#        (?\w+?\s\d+)\s+process\s+\d+\s+")
        #reThread = re.compile("\*?\s+(\d+)\s+process\s+\d+\s+")
        lines = gdb.communicate("info threads")
        threads = []
        for line in lines:
            rexp = reThread.match(line)
            if rexp:
                threads.append(rexp.group(1))
        if threads:
            logging.debug("Found threads")
            logging.log(logging.INSANE, "Threads found: %s" %(threads))
        else:
            logging.warn("No threads found")
    
        #Iterate over the threads
        while threads:
            thread = threads.pop()
            logging.debug("Now searching thread %s" %(thread))
    
            #Enter the thread
            lines = gdb.communicate("thread %s" %(thread))
            inThread = False
            for line in lines:
                #if re.match("\[Switching to thread\s+%s\s+\(process\s+\d+\)\]" %thread, line):
                if re.match("\[Switching to thread\s+%s" %thread, line):
                    inThread = True
                    break
            if inThread:
                logging.debug("Successfully entered thread %s" %(thread))
            else:
                logging.warn("Cannot enter thread")
                continue
    
    
            #Get frames for this thread
            lines = gdb.communicate("info stack")
            frames = []
            for line in lines:
                rexp = CoreFile.__reFrame.match(line)
                if rexp:
                    frames.append(rexp.group(1))
            if frames:
                logging.debug("Found frames")
                logging.log(logging.INSANE, "Frames found: %s" %(frames))
            else:
                logging.warn("No frames found")
    
            #Step through the frames
            while frames:
                frame = frames.pop()
                logging.debug("Now searching frame %s" %(frame))
    
                #Enter the frame
                lines = gdb.communicate("frame %s"%(frame))
                inFrame = False
                for line in lines:
                    rexp = CoreFile.__reFrame.match(line)
                    if rexp and rexp.group(1) == frame:
                        inFrame = True
                        break
                if inFrame:
                    logging.debug("Successfully entered frame %s" %(frame))
                else:
                    logging.warn("Cannot enter frame")
                    continue
    
                #List the first line of source so we can do a forward-search in gdb
                lines = gdb.communicate("info line 1")
                hasSource = False
                for line in lines:
                    if 'Line 1 of "' in line:
                        hasSource = True
                        break
                if hasSource:
                    logging.debug("Frame has source")
                else:
                    logging.warn("Frame has no source")
                    continue
    
                #Search the source for a call to funcName
                """NOTE: This doesn't search the first line, however, it is safe to assume that
                         it wouldn't be on the first line"""
                keepSearching = True
                varNames = []
                reVars = re.compile(r"(\d+)\s*%s\s*\((.*)\)\s*;" %(funcName))
                while keepSearching:
                    lines = gdb.communicate("forward-search \s*%s\s*(.*)\s*;" %(funcName))
                    for line in lines:
                        if 'Expression not found' in line:
                            keepSearching = False
                            break
                        else:
                            rexp = reVars.search(line)
                            if rexp:
                                vars = rexp.group(2).replace('&','').replace('*','').replace(' ','').replace('\t','').split(',')  #HACK:  Ugly, ugly, ugly
                                if varPosition < len(vars):
                                    varNames.append( vars[varPosition] )
                                    break
                if len(varNames) > 0:
                    logging.debug("varNames = %s" %(varNames))
                else:
                    logging.warn("No variable names were recovered")
                    continue
    
                #Get the values of the local variables, so we can return the variable value
                lines = gdb.communicate("info locals")
                varValue = ""
                for line in lines:
                    for varName in varNames:
                        if varName in line:
                            rexp = re.search("\s*%s\s+=\s+(\S+.*)"%(varName), line)
                            if rexp:
                                varValue = rexp.group(1)
                                break
                    if varValue:
                        break
    
                if varValue:
                    logging.debug("varValue = %s"%(varValue))
                    return varValue
                else:
                    logging.warn("Getting a value for varValue failed")
                    continue
        return None

    def processCore(self):
        """Processes the core file"""
  
        #Set up the options variables
        withline =   CoreFile.__options["withline"]
        force =      CoreFile.__options["force"]
        directory =  CoreFile.__options["directory"]
        coredir =    (CoreFile.__options["coredir"])    and CoreFile.__options["coredir"]    or directory
        exedir =     (CoreFile.__options["exedir"])     and CoreFile.__options["exedir"]     or directory
        sourcepath = (CoreFile.__options["sourcepath"]) and CoreFile.__options["sourcepath"] or ''
        objectpath = (CoreFile.__options["objectpath"]) and CoreFile.__options["objectpath"] or ''
  
        #Open gdb against the core file
        logging.info("Connecting gdb to the core file (%s)"%self.coreData['coreFile'])
        gdb = Gdb(CoreFile.__options)
        lines = gdb.open(self.coreData['coreFile'], None)
  
        #Find the executable that generated the core file to begin with
        executable = ''
        for line in lines:
            if 'Core was generated by' in line:
                rexp = re.search(r"Core was generated by `(.+)'\.", line)
                if rexp: executable = rexp.group(1).split()[0]
        if not executable:
            logging.critical("Error: Cannot discover executable that generated core file %s" %self.coreData['coreFile'])
            return False
  
        #Exit gdb
        logging.info("Disconnecting gdb from the core file (%s)"%self.coreData['coreFile'])
        gdb.close()
  
        #Reconnect to gdb using executable
        logging.info("Reconnecting gdb to the core file (%s) AND the executable (%s)"%(self.coreData['coreFile'],executable))
        lines = gdb.open(self.coreData['coreFile'], executable)
  
        #Check for gdb errors
        logging.debug("Checking for gdb errors")
        symbolsLoaded = True
        for line in lines:
            if '(no debugging symbols found)' in line:
                pass # some .so's may have this message, which may be OK
                #symbolsLoaded = False
                #if withline:
                #  logging.critical("The --withlines argument was specified, however there is no symbol table compiled into the executable.\n" +
                #                 "Try running without this argument.")
                #  if not force:
                #      sys.exit(2)
            elif 'warning: exec file is newer than core file.' in line:
                logging.critical("GDB: The executable (%s/%s) is newer than the core file (%s/%s).\n" %(exedir,executable, coredir,self.coreData['coreFile']) +
                               "      Try using touch to change the timestamps.")
                if not force:
                    sys.exit(2)
            elif 'warning: core file may not match specified executable file.' in line:
                logging.critical("GDB: The executable (%s/%s) doesn't match the core file (%s/%s).\n" %(exedir,executable, coredir,self.coreData['coreFile']))
                if not force:
                    sys.exit(2)
            elif "%s: No such file or directory."%(executable) in line:
                logging.critical("GDB: The executable (%s/%s) doesn't exist." %(exedir,executable))
                if not force:
                    sys.exit(2)
  
        if symbolsLoaded:
            #Find the value for the returned size from MPI_Comm_size()
            #Currently removed for speed, we aren't using it right now
            """
            if self.coreData['rankSize'] is None:
              logging.debug("Find a value for the highest rank")
              rankSizeValue = self.getFunctionValue(gdb, 'MPI_Comm_size', 1)
              if rankSizeValue and rankSizeValue.isdigit() and int(rankSizeValue) >= 0:
                self.coreData['rankSize'] = int(rankSizeValue)
                logging.debug("Value found from stack, using: %i" %(self.coreData['rankSize']))
              else:
                logging.info("Could not find a highest rank value")
            """
  
            #Find the value for the returned rank from MPI_Comm_rank()
            logging.debug("Find a value for the current rank")
            rankValue = self.getFunctionValue(gdb, 'MPI_Comm_rank', 1)
            rankValue = "-1"
            if rankValue and rankValue.isdigit() and int(rankValue) >= 0:
                self.coreData['rank'] = int(rankValue)
                logging.debug("Value found from stack, using: %i" %(self.coreData['rank']))
            else:
                global next_rank
                while next_rank in rank_list:
                    next_rank += 1
                logging.info("Could not find a rank value, using default: %i" %(self.coreData['rank']))
            rank_list.append(self.coreData['rank'])
  
        ### PARSE THE STACK TRACE ###
        logging.debug("Parsing the stack trace")
  
        #Get stack trace from all threads
        lines = gdb.communicate("thread apply all bt")
  
        #Ensure we have a stack trace to parse
        if not lines:
            logging.critical("No backtrace returned")
            if not force:
                sys.exit(2)
  
  
        #Iterate over each line and find what we need
        inThread, functions = False, []
        for line in lines:
            #Start a new Thread listing
            if 'Thread' in line:
                logging.debug("functions %s" %functions)
                self.addFunctions(functions)
                logging.debug("Found new thread")
                inThread,functions = True,[]
  
            #In some cases, gdb will quit the stack trace early
            elif 'Backtrace stopped: ' in line:
                logging.critical("GDB: Backtrace stopped")
                if not force:
                    sys.exit(2)
  
            #If we're in a Thread listing, see if we match a stack frame
            elif inThread:
                rexp = CoreFile.__reFrame.match(line)
                if rexp:
                    if withline and rexp.group(3):
                        function = "%s@%s" %(rexp.group(2),rexp.group(3))
                    else:
                        function = rexp.group(2)
                    logging.log(logging.INSANE, "Found function %s from %s"%(function, line))  #This is a very verbose debug!
                    functions.append( function )
  
        #Merge this process with the rest
        self.addFunctions(functions)
  
        #Exit gdb
        logging.info("Disconnecting gdb from the core file (%s) AND the executable (%s)"%(self.coreData['coreFile'],executable))
        gdb.close()
  
        return True


###############################################################################
class CoreMergerArgs(StatMergerArgs):
    def __init__(self):
        StatMergerArgs.__init__(self)
        
        self.arg_map["output"] = StatMergerArgs.StatMergerArgElement("o", True, str, "coredump.dot", "the output filename")
        self.arg_map["pattern"] = StatMergerArgs.StatMergerArgElement("p", True, str, "^core.[0-9]+$", "the core file regex pattern")
        self.arg_map["directory"] = StatMergerArgs.StatMergerArgElement("d", True, str, os.getcwd(), "the output directory")
        self.arg_map["coredir"] = StatMergerArgs.StatMergerArgElement("D", True, str, os.getcwd(), "the core file directory")
        self.arg_map["exedir"] = StatMergerArgs.StatMergerArgElement("e", True, str, os.getcwd(), "the exe file directory")
        self.arg_map["sourcepath"] = StatMergerArgs.StatMergerArgElement("s", True, str, os.getcwd(), "the source file directory")
        self.arg_map["objectpath"] = StatMergerArgs.StatMergerArgElement("O", True, str, os.getcwd(), "the object file directory")
        self.arg_map["loglevel"] = StatMergerArgs.StatMergerArgElement("L", True, str, "error", "the verbosity level (critical|error|warning|info|verbose|debug|insane)")
        self.arg_map["logfile"] = StatMergerArgs.StatMergerArgElement("F", True, str, "stdout", "the log file name (defaults to stdout)")
        self.arg_map["withline"] = StatMergerArgs.StatMergerArgElement("i", False, int, 0, "whether to gather source line number")
        self.arg_map["force"] = StatMergerArgs.StatMergerArgElement("r", False, int, 0, "whether to force parsing on warnings and errors")
        self.arg_map["threads"] = StatMergerArgs.StatMergerArgElement("T", False, int, 1, "max number of threads")
        self.arg_map["fileprefix"] = self.StatMergerArgElement("f", True, str, "NULL", "[LW] the output filename prefix")
        self.arg_map["type"] = self.StatMergerArgElement("t", True, str, "dot", "[LW] the output file type (raw or dot)")
        self.arg_map["high"] = self.StatMergerArgElement("H", True, int, -1, "[LW] the high rank of the input traces")
        self.arg_map["limit"] = self.StatMergerArgElement("l", True, int, 8192, "[LW] the max number of traces to parse per process")

        self.usage_msg_synopsis = '\nThis tool will merge the stack traces from the user-specified core files and output 2 .dot files, one with just function names, the other with function names + line number information\n\nNOTE: for lightweight core files, the -x option is required.  Options marked with "[LW]" are for lightweight core files\n'
        self.usage_msg_command = '\nUSAGE:\n\tpython %s [options] -x <exe_path> -c <corefile>*\n\tpython %s [options] -x <exe_path> -c <core_files_dir>\n\tpython %s [options] -c <corefile>*\n' % (sys.argv[0], sys.argv[0], sys.argv[0])
        self.usage_msg_examples = '\nEXAMPLES:\n\tpython %s -x a.out -c core.0 core.1\n\tpython %s -x a.out -c core.*\n\tpython %s -x a.out -c ./\n\tpython %s -x a.out -c core_dir\n\tpython %s -c *.core\n' % (sys.argv[0], sys.argv[0], sys.argv[0], sys.argv[0], sys.argv[0])

    def is_valid_file(self, file_path):
        if file_path.find('.core') == 0:
            return True
        return False

    def error_check(self, options):
        StatMergerArgs.error_check(self, options)
        new_loglevel = initLogging(options["loglevel"], options["logfile"])    #Initialize the logging module
        options["loglevel"] = new_loglevel


class CoreTrace(StatTrace):
    def get_traces(self):
        self.rank = 0
        line_number_traces = []
        function_only_traces = []
        core_file = CoreFile(self.file_path, self.options)
        core_file.processCore()
        trace = core_file.coreData["traces"]
        self.rank = core_file.coreData["rank"]
        function_only_traces = trace

        self.options["withline"] = 1
        core_file2 = CoreFile(self.file_path, self.options)
        core_file2.processCore()
        trace2 = core_file2.coreData["traces"]
        self.options["withline"] = 0
        line_number_traces = trace2

        return [function_only_traces, line_number_traces]


class CoreMerger(StatMerger):
    def get_high_rank(self, trace_files):
        # determine the highest ranked task for graphlib initialization
        high_rank = 0
        high_rank = len(trace_files)
        high_rank = 9999999
        return high_rank


def initLogging(input_loglevel, input_logfile):
    """Initialize the logging module"""

    logging.VERBOSE = 25
    logging.addLevelName(logging.VERBOSE, 'VERBOSE')
    logging.INSANE = 5
    logging.addLevelName(logging.INSANE, 'INSANE')

    logLevels = {'debug': logging.DEBUG,
                 'info': logging.INFO,
                 'verbose': logging.VERBOSE,
                 'warning': logging.WARNING,
                 'error': logging.ERROR,
                 'insane': logging.INSANE,
                 'critical': logging.CRITICAL}
    logStreams = {'stdout': sys.stdout, 'stderr': sys.stderr}

    #NOTE: We're changing the options value from a string name to an int... this gets passed on to everything afterward!
    input_loglevel = logLevels.get(input_loglevel, logging.NOTSET)

    logFormat = '%(relativeCreated)-8d %(module)-12s:%(lineno)-5s %(levelname)-8s (%(name)s) %(message)s'
    logDate = None

    if input_logfile == 'stdout' or input_logfile == 'stderr':
        logStream = logStreams.get(input_logfile, logging.NOTSET)
        logFile = None
    else:
        logStream = None
        logFile = input_logfile

    logging.basicConfig( level=input_loglevel, format=logFormat, datefmt=logDate, filename=logFile, stream=logStream, filemode='w')
    logging.getLogger().name = "MainThread"
    logging.log( logging.VERBOSE, "Processing started at %s"%(datetime.now()) )
    return input_loglevel


###############################################################################
if __name__ == '__main__':
    core_file_type = 'full'
    try:
        file = sys.argv[sys.argv.index("-c") + 1]
        if os.path.isdir(file):
            for file_path in os.listdir(file):
                full_path = file + '/' +  file_path
                if full_path.find('core') != -1 and not os.path.isdir(full_path):
                    file = full_path
                    break
        f = open(file, "r")
        line = f.readline()
        if line.find("LIGHTWEIGHT COREFILE") != -1 or line.find("Summary") != -1:
            core_file_type = 'lightweight'
        else:
            core_file_type = 'full'
    except Exception as e:
        sys.stderr.write('failed to determine core file type: %s\n' %e)
    if core_file_type == 'lightweight':
        merger = BgCoreMerger(BgCoreTrace, BgCoreMergerArgs)
    else:
        merger = CoreMerger(CoreTrace, CoreMergerArgs)
    ret = merger.run()
    if ret != 0:
        sys.stderr.write('Merger failed\n')
        sys.exit(ret)
