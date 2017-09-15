import subprocess
import sys
import signal
import os
import logging
from datetime import datetime

def init_logging(input_loglevel, input_logfile):
    """Initialize the logging module"""

    logLevels = {'debug': logging.DEBUG,
                 'info': logging.INFO,
                 'warning': logging.WARNING,
                 'error': logging.ERROR,
                 'critical': logging.CRITICAL}
    logStreams = {'stdout': sys.stdout, 'stderr': sys.stderr}

    #NOTE: We're changing the options value from a string name to an int... this gets passed on to everything afterward!
    input_loglevel = logLevels.get(input_loglevel, logging.NOTSET)

    log_format = '%(relativeCreated)-8d %(filename)-12s:%(lineno)-5s %(levelname)-8s %(message)s'
    log_date = None

    if input_logfile == 'stdout' or input_logfile == 'stderr':
        log_stream = logStreams.get(input_logfile, logging.NOTSET)
        log_file = None
    else:
        log_stream = None
        log_file = input_logfile

    logging.basicConfig(level=input_loglevel, format=log_format, datefmt=log_date, filename=log_file, stream=log_stream, filemode='w')
    logging.getLogger().name = "MainThread"
    logging.log(logging.INFO, "Processing started at %s" %(datetime.now()))
    return input_loglevel


class GdbDriver(object):
    """A class to drive GDB"""
    input_prompt = '(gdb)'
    gdb_command = 'gdb'
    gdb_args = []

    def __init__(self, pid, log_level='error', log_file='stderr'):
        """Constructor"""
        init_logging(log_level, log_file)
        logging.info('GDB init')
        self.pid = pid

    def launch(self):
        """Launch the gdb process"""
        logging.debug('launching "%s %s"' %(self.gdb_command, repr(self.gdb_args)))
        try:
            self.subprocess = subprocess.Popen([self.gdb_command] + self.gdb_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        except Exception as e:
            logging.error('Failed to launch "%s %s": %s' %(self.gdb_command, repr(self.gdb_args), repr(e)))
            return False
        lines = self.readlines()
        logging.debug('%s' %repr(lines))
        return True

    def close(self):
        """Closes the gdb subprocess"""
        try:
            self.subprocess.communicate("quit\n")
            self.subprocess = None
        except:
            pass

    def __del__(self):
        """Destructor"""
        self.close()

    def readlines(self, breakstring=None):
        """Simply reads the lines, until we get to the '(gdb)' prompt
           prevents blocking from a lack of EOF with a stdout.readline() loop"""
        line = ''
        lines = []
        while True:
            ch = self.subprocess.stdout.read(1)
            if breakstring:
                if breakstring in line:
                    lines.append(line)
                    break
            if ch == '\n':
                lines.append(line)
                ch = ''
                line = ''
            elif ch == ' ': # Check for the prompt, and return if we get it
                if self.input_prompt in line:
                    logging.debug('gdb prompt break\n')
                    break
            line += ch
        return lines

    def communicate(self, command):
        """Sends the command to gdb, and returns a list of outputted lines
           \param command the command to send to gdb
           \returns a list of lines from a merged stdout/stderr stream"""
        if not command.endswith('\n'):
            command += '\n'
        logging.debug('sending command %s\n' %(command))
        self.subprocess.stdin.write(command)
        return self.readlines()

    def attach(self):
        """Attaches to the target process"""
        logging.info('GDB attach to PID %d' %(self.pid))
        lines = self.communicate("attach %d" %self.pid)
        logging.debug('%s' %(repr(lines)))
        return True

    def detach(self):
        """Detaches from the target process"""
        logging.info('GDB detach from PID %d' %(self.pid))
        lines = self.communicate("detach")
        logging.debug('%s' %(repr(lines)))
        return True

    def kill(self):
        """Kills the debug target process"""
        logging.info('killing PID %d' %(self.pid))
        lines = self.communicate("kill")
        logging.debug('%s' %(repr(lines)))
        return True

    def get_thread_list(self):
        """Gets the list of threads in the target process"""
        logging.info('GDB get thread list')
        tids = []
        lines = self.communicate("info threads")
        logging.debug('%s' %(repr(lines)))
        for line in lines:
            if line[0] == '*':
                line = line[1:]
            line = line.split()
            try:
                if line[0].isdigit():
                    tids.append(int(line[0]))
            except Exception as e:
                logging.warning('Failed to get thread list from "%s": %s' %(line, repr(e)))
                pass
        return tids

    def thread_focus(self, thread_id):
        """Sets the thread focus to the requested thread id"""
        logging.info('GDB thread focus ID %d' %(thread_id))
        lines = self.communicate("thread %d" %thread_id)
        logging.debug('%s' %(repr(lines)))
        return True

    def bt(self, thread_id):
        """Gets a backtrace from the requested thread id.
           returns list of frames, where each frame is a map of attributes"""
        logging.info('GDB thread bt ID %d' %(thread_id))
        ret = []
        lines = self.communicate("thread apply %d bt" %thread_id)
        logging.debug('%s' %(repr(lines)))
        lines = lines[2:]
        patched_lines = []
        # some frames may span multiple lines
        # each frame begins with #<frame_number>
        for i, line in enumerate(lines):
            if line[0] != '#':
                continue
            new_line = line
            j = i + 1
            while j < len(lines) and lines[j][0] != '#':
                new_line += lines[j]
                j += 1
            patched_lines.append(new_line)
        for line in patched_lines:
            if line[0] == '*':
                line = line[2:]
            split_line = line.split()
            error = False; function = '??'; source = '??'; linenum = 0

            # TODO: the in at and from search may not be ideal, since those strings may appear in a function name

            try:
                if ('in' in split_line):
                    #function = split_line[split_line.index('in') + 1] # do we want to strip off args
                    function = split_line[split_line.index('in') + 1].split('(')[0]
            except Exception as e:
                logging.warning('Failed to get function from "%s": %s' %(line, repr(e)))
                error = True
            try:
                if 'at' in  split_line:
                    source_line = split_line[split_line.index('at') + 1].split(':')
                    source = source_line[0]
                    linenum = int(source_line[1])
                elif 'from' in split_line:
                    source = split_line[split_line.index('from') + 1]
            except Exception as e:
                logging.warning('Failed to get source line from "%s": %s' %(line, repr(e)))
                error = True
            ret.append({'function':function, 'source':source, 'linenum':linenum, 'error':error})
        return ret

    def resume(self):
        """Resumes the debug target process"""
        logging.info('GDB resume PID %d' %(self.pid))
        ret = self.subprocess.stdin.write("continue\n")
        lines = self.readlines('Continuing')
        logging.debug('%s' %(repr(lines)))
        return True

    def pause(self):
        """Pauses the debug target process"""
        logging.info('GDB pause PID %d' %(self.pid))
        ret = os.kill(self.pid, signal.SIGINT)
        lines = self.readlines()
        logging.debug('%s' %(repr(lines)))
        return True


if __name__ == "__main__":
    gdb = GdbDriver(int(sys.argv[1]), 'debug', 'log.txt')
    if gdb.launch() is False:
        sys.stderr.write('gdb launch failed\n')
        sys.exit(1)
    gdb.attach()
    tids = gdb.get_thread_list()
    tids.sort()
    for thread_id in tids:
        bt = gdb.bt(thread_id)
        for frame in bt:
            print frame
        print
    gdb.resume()
    gdb.pause()
    bt = gdb.bt(1)
    for frame in bt:
        print frame
    print
#    gdb.kill()
    print '\ndetaching'
    gdb.detach()
