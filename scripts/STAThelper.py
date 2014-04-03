"""@package STAThelper
Helper routines for STAT and STATview."""

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

import os.path

## A variable to determine whther we have the pygments module for syntax hilighting
HAVE_PYGMENTS = True
try:
    from pygments.formatter import Formatter
except:
    HAVE_PYGMENTS = False

## A list of MPI function names, for the "Hide MPI" feature
MPI_FUNCTIONS = ['mpi_file_iwrite_shared', 'mpi_info_set', 'mpio_request_c2f',
                 'mpi_file_open', 'mpi_init', 'mpio_request_f2c',
                 'mpi_file_preallocate', 'mpi_init_thread', 'mpio_test',
                 'mpi_file_read', 'mpi_initialized', 'mpio_wait',
                 'mpi_file_read_all', 'mpi_int2handle', 'mpi_abort',
                 'mpi_file_read_all_begin', 'mpi_intercomm_create',
                 'mpi_address', 'mpi_file_read_all_end', 'mpi_intercomm_merge',
                 'mpi_allgather', 'mpi_file_read_at', 'mpi_iprobe',
                 'mpi_allgatherv', 'mpi_file_read_at_all', 'mpi_irecv',
                 'mpi_allreduce', 'mpi_file_read_at_all_begin', 'mpi_irsend',
                 'mpi_alltoall', 'mpi_file_read_at_all_end', 'mpi_isend',
                 'mpi_alltoallv', 'mpi_file_read_ordered', 'mpi_issend',
                 'mpi_attr_delete', 'mpi_file_read_ordered_begin',
                 'mpi_keyval_create', 'mpi_attr_get',
                 'mpi_file_read_ordered_end', 'mpi_keyval_free',
                 'mpi_attr_put', 'mpi_file_read_shared', 'mpi_null_copy_fn',
                 'mpi_barrier', 'mpi_file_seek', 'mpi_null_delete_fn',
                 'mpi_bcast', 'mpi_file_seek_shared', 'mpi_op_create',
                 'mpi_bsend', 'mpi_file_set_atomicity', 'mpi_op_free',
                 'mpi_bsend_init', 'mpi_file_set_errhandler', 'mpi_pack',
                 'mpi_buffer_attach', 'mpi_file_set_info', 'mpi_pack_size',
                 'mpi_buffer_detach', 'mpi_file_set_size', 'mpi_pcontrol',
                 'mpi_char', 'mpi_file_set_view', 'mpi_probe', 'mpi_cancel',
                 'mpi_file_sync', 'mpi_recv', 'mpi_cart_coords',
                 'mpi_file_write', 'mpi_recv_init', 'mpi_cart_create',
                 'mpi_file_write_all', 'mpi_reduce', 'mpi_cart_get',
                 'mpi_file_write_all_begin', 'mpi_reduce_scatter',
                 'mpi_cart_map', 'mpi_file_write_all_end', 'mpi_request_c2f',
                 'mpi_cart_rank', 'mpi_file_write_at', 'mpi_request_free',
                 'mpi_cart_shift', 'mpi_file_write_at_all', 'mpi_rsend',
                 'mpi_cart_sub', 'mpi_file_write_at_all_begin',
                 'mpi_rsend_init', 'mpi_cartdim_get',
                 'mpi_file_write_at_all_end', 'mpi_scan', 'mpi_comm_compare',
                 'mpi_file_write_ordered', 'mpi_scatter', 'mpi_comm_create',
                 'mpi_file_write_ordered_begin', 'mpi_scatterv',
                 'mpi_comm_dup', 'mpi_file_write_ordered_end', 'mpi_send',
                 'mpi_comm_free', 'mpi_file_write_shared', 'mpi_send_init',
                 'mpi_comm_get_name', 'mpi_finalize', 'mpi_sendrecv',
                 'mpi_comm_group', 'mpi_finalized', 'mpi_sendrecv_replace',
                 'mpi_comm_rank', 'mpi_gather', 'mpi_ssend',
                 'mpi_comm_remote_group', 'mpi_gatherv', 'mpi_ssend_init',
                 'mpi_comm_remote_size', 'mpi_get_count', 'mpi_start',
                 'mpi_comm_set_name', 'mpi_get_elements', 'mpi_startall',
                 'mpi_comm_size', 'mpi_get_processor_name', 'mpi_status_c2f',
                 'mpi_comm_split', 'mpi_getVersion',
                 'mpi_status_set_cancelled', 'mpi_comm_test_inter',
                 'mpi_graph_create', 'mpi_status_set_elements', 'mpi_dup_fn',
                 'mpi_graph_get', 'mpi_test', 'mpi_dims_create',
                 'mpi_graph_map', 'mpi_test_cancelled',
                 'mpi_errhandler_create', 'mpi_graph_neighbors', 'mpi_testall',
                 'mpi_errhandler_free', 'mpi_graph_neighbors_count',
                 'mpi_testany', 'mpi_errhandler_get', 'mpi_graphdims_get',
                 'mpi_testsome', 'mpi_errhandler_set', 'mpi_group_compare',
                 'mpi_topo_test', 'mpi_error_class', 'mpi_group_difference',
                 'mpi_type_commit', 'mpi_error_string', 'mpi_group_excl',
                 'mpi_type_contiguous', 'mpi_file_c2f', 'mpi_group_free',
                 'mpi_type_create_darray', 'mpi_file_close', 'mpi_group_incl',
                 'mpi_type_create_subarray', 'mpi_file_delete',
                 'mpi_group_intersection', 'mpi_type_extent', 'mpi_file_f2c',
                 'mpi_group_range_excl', 'mpi_type_free', 'mpi_file_get_amode',
                 'mpi_group_range_incl', 'mpi_type_get_contents',
                 'mpi_file_get_atomicity', 'mpi_group_rank',
                 'mpi_type_get_envelope', 'mpi_file_get_byte_offset',
                 'mpi_group_size', 'mpi_type_hvector',
                 'mpi_file_get_errhandler', 'mpi_group_translate_ranks',
                 'mpi_type_lb', 'mpi_file_get_group', 'mpi_group_union',
                 'mpi_type_size', 'mpi_file_get_info', 'mpi_ibsend',
                 'mpi_type_struct', 'mpi_file_get_position', 'mpi_info_c2f',
                 'mpi_type_ub', 'mpi_file_get_position_shared',
                 'mpi_info_create', 'mpi_type_vector', 'mpi_file_get_size',
                 'mpi_info_delete', 'mpi_unpack', 'mpi_file_get_type_extent',
                 'mpi_info_dup', 'mpi_wait', 'mpi_file_get_view',
                 'mpi_info_f2c', 'mpi_waitall', 'mpi_file_iread',
                 'mpi_info_free', 'mpi_waitany', 'mpi_file_iread_at',
                 'mpi_info_get', 'mpi_waitsome', 'mpi_file_iread_shared',
                 'mpi_info_get_nkeys', 'mpi_wtick', 'mpi_file_iwrite',
                 'mpi_info_get_nthkey', 'mpi_wtime', 'mpi_file_iwrite_at',
                 'mpi_info_get_valuelen', 'mpi_file_iwrite_shared',
                 'mpi_info_set']
## A global variable to store to pygments highlighted source lines
pygments_lines = []

if HAVE_PYGMENTS:
    ## Formatter for syntax highlighting the source view window.
    class STATviewFormatter(Formatter):
        """Formatter for syntax highlighting the source view window."""

        def __init__(self, **options):
            """Initialize the style types."""
            Formatter.__init__(self, **options)
            self.styles = {}
            for token, style in self.style:
                start = ''
                end = ''
                newline_end = '\n'
                newline_start = ''
                if style['color']:
                    start += newline_start + '<STATviewcolor=#%s>' % style['color'] + '\n'
                    newline_start = ''
                if style['bold']:
                    start += newline_start + '<STATviewb>\n'
                    newline_start = ''
                if style['italic']:
                    start += newline_start + '<STATviewi>\n'
                    newline_start = ''
                if style['underline']:
                    start += newline_start + '<STATviewu>\n'
                    newline_start = ''
                if style['underline']:
                    end = newline_end + end + '</STATviewu>\n'
                    newline_end = ''
                if style['italic']:
                    end = newline_end + end + '</STATviewi>\n'
                    newline_end = ''
                if style['bold']:
                    end = newline_end + end + '</STATviewb>\n'
                    newline_end = ''
                if style['color']:
                    end = newline_end + end + '</STATviewcolor>\n'
                    newline_end = ''
                self.styles[token] = (start, end)

        def format(self, tokensource, outfile):
            """Define the output format, generate the pygments_list."""
            # parse step to generate structure
            current_line = []
            for ttype, value in tokensource:
                color = '#000000'
                bold = False
                italics = False
                underline = False
                begin, end = self.styles[ttype]
                if begin.find('<STATviewcolor=#') != -1:
                    color = begin[begin.find('<STATviewcolor=#') + 15:begin.find('<STATviewcolor=#')+22]
                if begin.find('<STATviewb>') != -1:
                    bold = True
                if begin.find('<STATviewi>') != -1:
                    italics = True
                if begin.find('<STATviewu>') != -1:
                    underline = True
                values = value.split('\n')
                count = -1
                for value in values:
                    count += 1
                    if count != len(values) - 1:
                        value = value + '\n'
                    current_line.append((value, (color, bold, italics, underline)))
                    if value != values[-1] and len(values) != 1:
                        pygments_lines.append(current_line)
                        current_line = []

            # parse step to generate dummy format
            for ttype, value in tokensource:
                outfile.write(value)


## The ProcTab class stores the process table
class ProcTab(object):
    """The ProcTab class stores the process table"""
    def __init__(self):
        self.launcher_host = None
        self.launcher_pid = None
        self.executable_path = None
        self.executable_paths = []
        self.process_list = []


def get_proctab(proctab_file_path):
    """Retrieve the proctab object from a process table file"""
    with open(proctab_file_path, 'r') as proctab_file:
        launcher = proctab_file.next().strip('\n').split(':')
        proctab = ProcTab()
        proctab.launcher_host = launcher[0]
        proctab.launcher_pid = int(launcher[1])
        for line in proctab_file:
            line = line.strip('\n').split()
            rank = int(line[0])
            host_pid = line[1].split(':')
            host = host_pid[0]
            pid = int(host_pid[1])
            exe = line[2]
            if exe in proctab.executable_paths:
                index = proctab.executable_paths.index(exe)
            else:
                index = len(proctab.executable_paths)
                proctab.executable_paths.append(exe)
            if proctab.executable_path is None:
                proctab.executable_path = exe
            proctab.process_list.append((rank, host, pid, index))
    return proctab


## \param function_name - the edge label string
#  \return true if the input function is an MPI function
#
#  \n
def is_mpi(function_name):
    """Determine if a function is an MPI function."""
    function_name = function_name.lower()
    if function_name[0] == 'p':  # check for PMPI wrapper name
        function_name = function_name[1:]
    if function_name in MPI_FUNCTIONS:
        return True
    return False


## \param label - the edge label string
#  \return a list of integer ranks
#
#  \n
def get_task_list(label):
    """Get an integer list of tasks from an edge label."""
    if label == '':
        return []
    if label[0] != '[':
        # this is just a count and representative
        label = label[label.find(':') + 2:label.find(']')]
        return [int(label)]
    else:
        # this is a full node list
        if label.find('[') != -1:
            label = label[1:-1]
    task_list = []
    if label == '':
        return task_list
    for element in label.split(','):
        dash_index = element.find('-')
        if dash_index != -1:
            start = int(element[:dash_index])
            end = int(element[dash_index + 1:])
            task_list += range(start, end + 1)
        else:
            task_list.append(int(element))
    return task_list


## \param executable - the executable to search for
#  \return the executable file path
#
#  \n
def which(executable):
    """Search directories in the $PATH to find the requested executable"""
    path = os.environ.get("PATH")
    for directory in path.split(':'):
        filepath = os.path.join(directory, executable)
        if os.access(filepath, os.X_OK):
            return filepath
    return None


## \param color - the input r, g, b, a color specification
#  \return a string representation of the color
#
#  \n
def color_to_string(color):
    """Translate a color specification to a color string."""
    r, g, b, a = color
    ret = '#'
    ret += "%02x" % int(r * 255 * a + 255 * (1.0 - a))
    ret += "%02x" % int(g * 255 * a + 255 * (1.0 - a))
    ret += "%02x" % int(b * 255 * a + 255 * (1.0 - a))
    return ret


## \param label - the stack frame text
#  \return True if the label includes source file and line number info
#
#  \n
def label_has_source(label):
    """return True if the label includes source file and line number info"""
    return label.find('@') != -1


## \param label - the stack frame text
#  \return True if the label includes source file and line number info and the node is not eq class collapsed
#
#  \n
def label_collapsed(label):
    """return True if the label includes source file and line number info and the node is not eq class collapsed"""
    return label.find('\\n') != -1


## \param label - the stack frame text
#  \return True if the label includes source file and line number info and the node is not eq class collapsed
#
#  \n
def has_source_and_not_collapsed(label):
    """return True if the label includes source file and line number info and the node is not eq class collapsed"""
    return label_has_source(label) and not label_collapsed(label)


## \param label - the stack frame text
## \param item - [optional] the item index
#  \return - a tuple of (function name, line number, variable info)
#
#  \n
def decompose_node(label, item=None):
    """Decompose a stack frame's text into individual components."""
    function_name = ''
    source_line = ''
    iter_string = ''
    if has_source_and_not_collapsed(label):
        function_name = label[:label.find('@')]
        if label.find('$') != -1 and label.find('$$') == -1:  # and clause for name mangling of C++ on BG/Q example
            source_line = label[label.find('@') + 1:label.find('$')]
            iter_string = label[label.find('$') + 1:]
        else:
            source_line = label[label.find('@') + 1:]
            iter_string = ''
    elif label_collapsed(label) and item is not None:
        if item == -1:
            return_list = []
            frames = label.split('\\n')
            for frame in frames:
                function_name, source_line, iter_string = decompose_node(frame)
                return_list.append((function_name, source_line, iter_string))
            return return_list
        else:
            function_name, source_line, iter_string = decompose_node(label.split('\\n')[item])
    else:
        function_name = label
    return function_name, source_line, iter_string


## \param var_spec - the variable specificaiton (location and name)
#  \return a string representation of the variable specificaiton
#
#  \n
def var_spec_to_string(var_spec):
    """Translates a variable specificaiton list into a string."""
    if var_spec == []:
        return 'NULL'
    ret = '%d#' % len(var_spec)
    for filename, line, depth, var in var_spec:
        ret += '%s:%d.%d$%s,' % (filename, line, depth, var)
    ret = ret[:len(ret) - 1]
    return ret


## \param label - the input label
#  \return a copy of the label with appropriate escape characters added
#
#  \n
def escaped_label(label):
    """return a copy of the label with appropriate escape characters added"""
    if label.find('<') == -1 and label.find('>') == -1:
        return label
    ret = ''
    prev = ' '
    for character in label:
        if prev != '\\' and (character == '<' or character == '>'):
            ret += '\\'
        ret += character
        prev = character
    return ret

#global DEBUG
#DEBUG = False
# #   debug_msg('entering function %s' %(inspect.stack()[0][3]))
#def debug_msg(msg):
#    if DEBUG == True:
#        sys.stdout.write('%s:%d: %s\n"' %(inspect.stack()[1][1], inspect.stack()[1][2], msg))
