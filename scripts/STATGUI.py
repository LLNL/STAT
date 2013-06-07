#!/bin/env python

"""@package STATGUI
A GUI for driving the Stack Trace Analysis Tool."""

__copyright__ = """Copyright (c) 2007-2013, Lawrence Livermore National Security, LLC."""
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
__version__ = "2.0.0"

import STAThelper
from STAThelper import var_spec_to_string, get_task_list
import STATview
from STATview import STATDotWindow, stat_wait_dialog, show_error_dialog, search_paths, STAT_LOGO
import sys, DLFCN
sys.setdlopenflags(DLFCN.RTLD_NOW | DLFCN.RTLD_GLOBAL)
from STAT import STAT_FrontEnd, intArray, STAT_LOG_NONE, STAT_LOG_FE, STAT_LOG_BE, STAT_LOG_CP, STAT_LOG_MRN, STAT_LOG_SW, STAT_LOG_SWERR, STAT_LOG_NONE, STAT_OK, STAT_APPLICATION_EXITED, STAT_VERBOSE_ERROR, STAT_VERBOSE_FULL, STAT_VERBOSE_STDOUT, STAT_TOPOLOGY_AUTO, STAT_TOPOLOGY_DEPTH, STAT_TOPOLOGY_FANOUT, STAT_TOPOLOGY_USER, STAT_PENDING_ACK, STAT_LAUNCH, STAT_ATTACH, STAT_SERIAL_ATTACH, STAT_SAMPLE_FUNCTION_ONLY, STAT_SAMPLE_LINE, STAT_SAMPLE_PC, STAT_SAMPLE_COUNT_REP, STAT_SAMPLE_THREADS, STAT_SAMPLE_CLEAR_ON_SAMPLE, STAT_SAMPLE_PYTHON
import commands
import subprocess
import shelve
import time
import string
import os
import gtk
import gobject
import re
from collections import defaultdict

## The ProcTab class stores the process table
class ProcTab(object):
    def __init__(self):
        self.launcher_host = None
        self.launcher_pid = None
        self.executable_path = None
        self.executable_paths = []
        self.process_list = []

## The STATGUI window adds STAT operations to the STATview window.
class STATGUI(STATDotWindow):
    """The STATGUI window adds STAT operations to the STATview window."""

    ui2 = ''
    ui2 += '<ui>\n'
    ui2 += '    <toolbar name="STAT_Actions">\n'
    ui2 += '        <toolitem action="Attach"/>\n'
    ui2 += '        <toolitem action="ReAttach"/>\n'
    ui2 += '        <toolitem action="Detach"/>\n'
    ui2 += '        <toolitem action="Pause"/>\n'
    ui2 += '        <toolitem action="Resume"/>\n'
#    ui2 += '        <toolitem action="Kill"/>\n'
    ui2 += '        <toolitem action="Sample"/>\n'
    ui2 += '        <toolitem action="SampleMultiple"/>\n'
    ui2 += '    </toolbar>\n'
    ui2 += '</ui>\n'

    def __init__(self):
        """The constructor.

        Initializes all instance varaibles and creates the GUI window.
        """
        if not os.path.exists('%s/.STAT' %os.environ['HOME']):
            try:
                os.mkdir('%s/.STAT' %os.environ['HOME'])
            except:
                pass
        self.STAT = STAT_FrontEnd()
        self.properties_window = None
        self.proctab_file_path = None
        self.proctab = None
        self.attached = False
        self.reattach = False
        self.serial_process_list = None
        self.serial_attach = False
        self.sample_task_list = ['Sample Stack Traces', 'Gather Stack Traces', 'Render Stack Traces']
        self.attach_task_list = ['Launch Daemons', 'Connect to Daemons', 'Attach to Application']
        self.attach_task_list += self.sample_task_list
        types = { 'Topology Type'     : ['automatic', 'depth', 'max fanout', 'custom'],
                  'Verbosity Type'    : ['error', 'stdout', 'full'],
                  'Sample Type'       : ['function only', 'function and pc', 'function and line'],
                  'Edge Type'         : ['full list', 'count and representative'],
                  'Remote Host Shell' : ['rsh', 'ssh'] }
        if not hasattr(self, "types"):
            self.types = {}
        for opt in types:
            self.types[opt] = types[opt]
        options = { 'Remote Host'                      : "localhost",
                    'Remote Host Shell'                : "rsh",
                    'PID'                              : None,
                    'Launcher Exe'                     : '',
                    'Serial PID'                       : None,
                    'Serial Exe'                       : '',
                    'Serial Process List'              : '',
                    'Topology Type'                    : 'automatic',
                    'Topology'                         : '1',
                    'Share App Nodes'                  : True,
                    'Tool Daemon Path'                 : self.STAT.getToolDaemonExe(),
                    'Filter Path'                      : self.STAT.getFilterPath(),
                    'Job Launcher'                     : 'mpirun|srun|orterun|aprun|runjob',
                    'Log Dir'                          : os.environ['HOME'],
                    'Log Frontend'                     : False,
                    'Log Backend'                      : False,
                    'Log CP'                           : False,
                    'Log SW'                           : False,
                    'Log SWERR'                        : False,
                    'Use MRNet Printf'                 : False,
                    'Verbosity Type'                   : 'error',
                    'Communication Nodes'              : '',
                    'Communication Processes per Node' : 8,
                    'Num Traces'                       : 10,
                    'Trace Frequency (ms)'             : 1000,
                    'Num Retries'                      : 5,
                    'Retry Frequency (us)'             : 10,
                    'With Threads'                     : False,
                    'Gather Python Traces'             : False,
                    'Clear On Sample'                  : True,
                    'Gather Individual Samples'        : False,
                    'Run Time Before Sample (sec)'     : 0,
                    'Sample Type'                      : 'function only',
                    'Edge Type'                        : 'full list',
                    'DDT Path'                         : STAThelper._which('ddt'),
                    'DDT LaunchMON Prefix'             : '/usr/local',
                    'TotalView Path'                   : STAThelper._which('totalview'),
                    'Additional Debugger Args'         : '' }
        if not hasattr(self, "options"):
            self.options = {}
        for option in options:
            self.options[option] = options[option]
        if 'STAT_LMON_DEBUG_BES' in os.environ:
            self.options['Debug Backends'] = True
        else:
            self.options['Debug Backends'] = False

        # Check for site default options then for user default options
        site_options_path = '%s/etc/STAT/STAT.conf' %self.STAT.getInstallPrefix()
        user_options_path = '%s/.STATrc' %(os.environ.get('HOME'))
        for path in [site_options_path, user_options_path]:
            if os.path.exists(path):
                try:
                    with open(path, 'r') as f:
                        for line in f:
                            if line[0] == '#':
                                continue
                            split_line = line.split('=')
                            if len(split_line) != 2:
                                sys.stderr.write('invalid preference specification %s in file %s\n' %(line.strip('\n'), path))
                                continue
                            option = string.lstrip(string.rstrip(split_line[0]))
                            value = string.lstrip(string.rstrip(split_line[1]))
                            if option in self.options.keys():
                                if type(self.options[option]) == int:
                                    value = int(value)
                                elif type(self.options[option]) == bool:
                                    if string.lower(value) == 'true':
                                        value = True
                                    elif string.lower(value) == 'false':
                                        value = False
                                    else:
                                        sys.stderr.write('invalid value %s for option %s as specified in file %s.  Expecting either "true" or "false".\n' %(value, option, path))
                                        continue
                                self.options[option] = value
                            elif option == 'Source Search Path':
                                if os.path.exists(value):
                                    search_paths['source'].append(value)
                                else:
                                    sys.stderr.write('search path %s specified in %s is not accessible\n' %(value, path))
                            elif option == 'Include Search Path':
                                if os.path.exists(value):
                                    search_paths['include'].append(value)
                                else:
                                    sys.stderr.write('search path %s specified in %s is not accessible\n' %(value, path))
                            else:
                                sys.stderr.write('invalid option %s in file %s\n' %(option, path))
                except IOError as e:
                    sys.stderr.write('%s\nfailed to open preferences file %s\n' %(repr(e), path))
                    continue
                except Exception as e:
                    import traceback
                    traceback.print_exc()
                    sys.stderr.write('%s\nfailed to process preferences file %s\n' %(repr(e), path))
                    continue

        self.var_spec = []
        self.filter_entry = None
        self.serial_filter_entry = None
        uimanager = gtk.UIManager()
        self.actiongroup = gtk.ActionGroup('Commands')
        actions = []
        actions.append(('Attach', gtk.STOCK_CONNECT, 'Attach', None, 'Attach to an application', self.on_attach))
        actions.append(('ReAttach', gtk.STOCK_CONNECT, 'ReAttach', None, 'Attach to the last application STAT detached from', lambda a: stat_wait_dialog.show_wait_dialog_and_run(self.on_reattach, (a,), self.attach_task_list, self)))
        actions.append(('Detach', gtk.STOCK_DISCONNECT, 'Detach', None, 'Detach from the application', lambda a: stat_wait_dialog.show_wait_dialog_and_run(self.on_detach, (a,), ["Detach From Application"], self)))
        actions.append(('Pause', gtk.STOCK_MEDIA_PAUSE, 'Pause', None, 'Pause the application', lambda a: stat_wait_dialog.show_wait_dialog_and_run(self.on_pause, (a,), ["Pause Application"], self)))
        actions.append(('Resume', gtk.STOCK_MEDIA_PLAY, 'Resume', None, 'Resume the application', lambda a: stat_wait_dialog.show_wait_dialog_and_run(self.on_resume, (a,), ['Resume Application'], self)))
#        actions.append(('Kill', gtk.STOCK_STOP, 'Kill', None, 'Kill the application', lambda a: stat_wait_dialog.show_wait_dialog_and_run(self.on_kill, (a,), ['Terminating Application'], self)))
        actions.append(('Sample', gtk.STOCK_FIND, 'Sample', None, 'Gather a stack trace sample from the application', lambda a: self.on_sample(a, False)))
        actions.append(('SampleMultiple', gtk.STOCK_ZOOM_IN, 'Sample\nMultiple', None, 'Gather the merged stack traces accumulated over time', lambda a:self.on_sample(a, True)))
        self.actiongroup.add_actions(actions)
        uimanager.insert_action_group(self.actiongroup, 0)
        self.set_action_sensitivity('new')
        uimanager.add_ui_from_string(self.ui2)
        toolbar = uimanager.get_widget('/STAT_Actions')
        toolbar.set_orientation(gtk.ORIENTATION_VERTICAL)
        self.hbox = gtk.HBox()
        self.hbox.pack_start(toolbar, False, False)
        lines = STATDotWindow.ui.split('\n')
        count = 0
        for line in lines:
            count += 1
            if line.find('menuitem action="SaveAs') != -1:
                break
        lines.insert(count, '            <menuitem action="Properties"/>')
        lines.insert(count, '            <separator/>')
        STATDotWindow.ui = ''
        for line in lines:
            STATDotWindow.ui += line + '\n'
        menu_actions = []
        menu_actions.append(('Properties', gtk.STOCK_PROPERTIES, 'P_roperties', '<control>R', 'View application properties', self.on_properties))
        menu_actions.append(('About', gtk.STOCK_ABOUT, None, None, None, self.on_about))
        STATDotWindow.__init__(self, menu_actions)
        self.set_title('STAT')
        help_text = """Search for processes on a
specified host, range of hosts, or
semi-colon separated list of hosts.
Example specifications:
host1
host1;host2
host[1-10]
host[1-10,12,15-20]
host[1-10,12,15-20];otherhost[30]
"""
        self.search_types.append(('hosts', self.search_hosts, help_text))
        self.on_attach(None)

    def search_hosts(self, text, match_case_check_box):
        """Callback to handle activation of focus task text entry."""
        if match_case_check_box.get_active() == False:
            text = text.lower()
        temp_host_list = text.replace(' ', '').split(';')
        host_list = []
        for host in temp_host_list:
            if host.find('[') != -1:
                prefix = host[0:host.find('[')]
                range = host[host.find('['):host.find(']') + 1]
                node_list = get_task_list(range)
                for node in node_list:
                    node_name = '%s%d' %(prefix, node)
                    host_list.append(node_name)
            else:
                host_list.append(host)
        ret = self.set_proctab()
        if ret == False:
            return False
        tasks = ''
        for rank, host, pid, exe_index in self.proctab.process_list:
            if match_case_check_box.get_active() == False:
                host = host.lower()
            if host in host_list:
                tasks += str(rank) + ','
        tasks = tasks[0:-1]
        self.get_current_graph().focus_tasks(tasks)
        return True


    def on_about(self, action):
        """Display info about STAT."""
        about_dialog = gtk.AboutDialog()
        about_dialog.set_name('STAT')
        about_dialog.set_authors(__author__)
        about_dialog.set_copyright(__copyright__)
        about_dialog.set_license(__license__)
        about_dialog.set_wrap_license(80)
        version = intArray(3)
        if self.STAT is None:
            self.STAT = STAT_FrontEnd()
        self.STAT.getVersion(version)
        about_dialog.set_version('%d.%d.%d' %(version[0], version[1], version[2]))
        try:
            pixbuf = gtk.gdk.pixbuf_new_from_file(STAT_LOGO)
            about_dialog.set_logo(pixbuf)
        except gobject.GError, error:
            pass
        about_dialog.set_website('https://github.com/lee218llnl/STAT')
        about_dialog.show_all()
        about_dialog.run()
        about_dialog.destroy()

    def set_action_sensitivity(self, state):
        """Set the sensitivity of the buttons."""
        for action in self.actiongroup.list_actions():
            action.set_sensitive(True)
            if action.get_name() in ['ReAttach', 'Detach', 'Pause', 'Resume', 'Kill', 'Sample', 'SampleMultiple', 'SampleMultiple'] and state == 'new':
                action.set_sensitive(False)
            elif action.get_name() in ['Detach', 'Pause', 'Resume', 'Kill', 'Sample', 'SampleMultiple', 'SampleMultiple'] and state == 'detached':
                action.set_sensitive(False)
            elif action.get_name() in ['Attach', 'ReAttach', 'Pause'] and state == 'paused':
                action.set_sensitive(False)
            elif action.get_name() in ['Attach', 'ReAttach', 'Resume'] and state == 'running':
                action.set_sensitive(False)
            elif state == 'busy':
                action.set_sensitive(False)

    def on_properties(self, action):
        """Display a window with application properties."""
        if self.attached == False:
            show_error_dialog('Application properties only available after attaching\n', self)
            return False
        if self.properties_window != None:
            self.properties_window.present()
            return True

        # gather application properties
        num_nodes = self.STAT.getNumApplNodes()
        num_procs = self.STAT.getNumApplProcs()
        appl_exe = self.STAT.getApplExe()
        ret = self.set_proctab()
        if ret == False:
            show_error_dialog('Failed to set process table file\n', self)
            return False

        process_table = ''
        job_launcher = "%s:%d" %(self.proctab.launcher_host, self.proctab.launcher_pid)
        entries = range(len(self.proctab.process_list))
        for rank, host, pid, exe_index in self.proctab.process_list:
            entries[rank] = '%d %s:%d %d\n' %(rank, host, pid, exe_index)
        for entry in entries:
            process_table += entry

        self.properties_window = gtk.Window()
        self.properties_window.set_title('Properties')
        self.properties_window.connect('destroy', self.on_properties_destroy)
        vbox = gtk.VBox()

        frame = gtk.Frame('Application Executable(s) (index:path)')
        text_view = gtk.TextView()
        text_buffer = gtk.TextBuffer()
        exes = ''
        for i in range(len(self.proctab.executable_paths)):
            exes += ('%d:%s\n' %(i, self.proctab.executable_paths[i]))
        text_buffer.set_text(exes)
        text_view.set_buffer(text_buffer)
        text_view.set_wrap_mode(False)
        text_view.set_editable(False)
        text_view.set_cursor_visible(False)
        frame.add(text_view)
        vbox.pack_start(frame, False, False, 0)

        frame = gtk.Frame('Number of application nodes')
        text_view = gtk.TextView()
        text_buffer = gtk.TextBuffer()
        text_buffer.set_text('%d' %num_nodes)
        text_view.set_buffer(text_buffer)
        text_view.set_wrap_mode(False)
        text_view.set_editable(False)
        text_view.set_cursor_visible(False)
        frame.add(text_view)
        vbox.pack_start(frame, False, False, 0)

        frame = gtk.Frame('Number of application processes')
        text_view = gtk.TextView()
        text_buffer = gtk.TextBuffer()
        text_buffer.set_text('%d' %num_procs)
        text_view.set_buffer(text_buffer)
        text_view.set_wrap_mode(False)
        text_view.set_editable(False)
        text_view.set_cursor_visible(False)
        frame.add(text_view)
        vbox.pack_start(frame, False, False, 0)

        if self.STAT.getApplicationOption() != STAT_SERIAL_ATTACH:
            frame = gtk.Frame('Job Launcher (host:PID)')
            text_view = gtk.TextView()
            text_buffer = gtk.TextBuffer()
            text_buffer.set_text(job_launcher)
            text_view.set_buffer(text_buffer)
            text_view.set_wrap_mode(False)
            text_view.set_editable(False)
            text_view.set_cursor_visible(False)
            frame.add(text_view)
            vbox.pack_start(frame, False, False, 0)

        frame = gtk.Frame('Process Table (rank host:PID exe_index)')
        text_view = gtk.TextView()
        text_view.set_size_request(400, 200)
        text_buffer = gtk.TextBuffer()
        text_buffer.set_text(process_table)
        text_view.set_buffer(text_buffer)
        text_view.set_wrap_mode(False)
        text_view.set_editable(False)
        text_view.set_cursor_visible(False)
        sw = gtk.ScrolledWindow()
        sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        sw.add(text_view)
        frame.add(sw)
        vbox.pack_start(frame, True, True, 0)
        global blahblah
        blahblah = text_buffer

        self.properties_window.add(vbox)
        self.properties_window.show_all()

    def on_properties_destroy(self, action):
        """Clean up the properties window."""
        global blahblah
        blahblah.set_text('')
        self.properties_window = None

    def pid_toggle_cb(self, action, pid, command):
        """A callback to modify the PID option."""
        self.options['PID'] = pid
        self.options['Launcher Exe'] = command

    def serial_pid_toggle_cb(self, action, pid, command):
        """A callback to modify the PID option."""
        self.options['Serial PID'] = pid
        self.options['Serial Exe'] = command

    def on_add_serial_process(self, widget, attach_dialog, serial_process_list_entry):
        text = serial_process_list_entry.get_text()
        text += ' %s@%s:%s ' %(self.options['Serial Exe'], self.options['Remote Host'], self.options['Serial PID'])
        text = serial_process_list_entry.set_text(text)
        attach_dialog.show_all()

    def on_update_process_listing(self, widget, frame, attach_dialog, filter=None):
        """Generate a wait dialog and search for user processes."""
        stat_wait_dialog.show_wait_dialog_and_run(self._on_update_process_listing, (widget, frame, attach_dialog, filter), [], attach_dialog)

    def on_update_serial_process_listing(self, widget, frame, attach_dialog, filter=None):
        """Generate a wait dialog and search for user processes."""
        stat_wait_dialog.show_wait_dialog_and_run(self._on_update_serial_process_listing, (widget, frame, attach_dialog, filter), [], attach_dialog)

    def _on_update_process_listing2(self, attach_dialog, filter, vbox, is_parallel):
        """Search for user processes."""
        self.options['Remote Host Shell'] = self.types['Remote Host Shell'][self.combo_boxes['Remote Host Shell'].get_active()]
        if self.options['Remote Host'] == 'localhost' or self.options['Remote Host'] == '':
            output = commands.getoutput('ps xw')
        else:
            output = commands.getoutput('%s %s ps xw' %(self.options['Remote Host Shell'], self.options['Remote Host']))
            if output.find('Hostname not found') != -1 or output.find('PID') == -1:
                show_error_dialog('Failed to get process listing for %s' %self.options['Remote Host'], attach_dialog)
                return False
        output = output.split('\n')
        pid_index = 0
        for line in output:
            if line.find('PID') != -1:
                break
            pid_index += 1
        output = output[pid_index:]
        line = output[0].split()
        pid_index = 0
        command_index = 0
        for counter, token in enumerate(line):
            if token == 'PID':
                pid_index = counter
            elif token == 'COMMAND':
                command_index = counter
        filter_compiled_re = re.compile(filter.get_text())
        started = False
        for line in output[1:]:
            try:
                text = '% 5d ' %int(line.split()[pid_index])
                for token in line.split()[command_index:]:
                    text += ' %s' %token
            except:
                continue
            if filter != None:
                if filter.get_text() != '':
                    if filter_compiled_re.search(text) is None:
                        continue
            if started == False:
                started = True
                radio_button = gtk.RadioButton(None, text)
                radio_button.set_active(True)
                if is_parallel:
                    self.options['PID'] = int(line.split()[pid_index])
                    self.options['Launcher Exe'] = line.split()[command_index]
                else:
                    self.options['Serial PID'] = int(line.split()[pid_index])
                    self.options['Serial Exe'] = line.split()[command_index]
            else:
                radio_button = gtk.RadioButton(radio_button, text)
            if is_parallel:
                radio_button.connect("toggled", self.pid_toggle_cb, int(line.split()[pid_index]), line.split()[command_index])
            else:
                radio_button.connect("toggled", self.serial_pid_toggle_cb, int(line.split()[pid_index]), line.split()[command_index])
            vbox.pack_start(radio_button, False, False, 0)


    def _on_update_process_listing(self, widget, frame, attach_dialog, filter=None):
        vbox = gtk.VBox()
        self._on_update_process_listing2(attach_dialog, filter, vbox, True)
        try:
            if self.sw != None:
                frame.remove(self.sw)
        except:
            pass
        self.sw = gtk.ScrolledWindow()
        self.sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        self.sw.add_with_viewport(vbox)
        frame.add(self.sw)
        attach_dialog.show_all()

    def _on_update_serial_process_listing(self, widget, frame, attach_dialog, filter=None):
        vbox = gtk.VBox()
        self._on_update_process_listing2(attach_dialog, filter, vbox, False)
        try:
            if self.serial_sw != None:
                frame.remove(self.serial_sw)
        except:
            pass
        self.serial_sw = gtk.ScrolledWindow()
        self.serial_sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        self.serial_sw.add_with_viewport(vbox)
        frame.add(self.serial_sw)
        attach_dialog.show_all()

    def on_update_remote_host(self, w, frame, attach_dialog, entry):
        """Callback to search a remote host for processes."""
        self.options['Remote Host'] = entry.get_text()
        entry.set_text(self.options['Remote Host'])
        self.on_update_process_listing(w, frame, attach_dialog, self.filter_entry)

    def on_update_serial_remote_host(self, w, frame, attach_dialog, entry):
        """Callback to search a remote host for processes."""
        self.options['Remote Host'] = entry.get_text()
        entry.set_text(self.options['Remote Host'])
        self.on_update_serial_process_listing(w, frame, attach_dialog, self.serial_filter_entry)

    def on_cancel_attach(self, widget, dialog):
        """Callback to handle canceling of attach dialog."""
        dialog.destroy()

    def manipulate_cb(self, widget, data, node):
        """Overloaded manipulate callback to handle variable search."""
        STATDotWindow.manipulate_cb(self, widget, data, node)
        if data == 'Temporally Order Children':
            self.check_for_loop_variable()

    def on_to_traverse_least_progress(self, action):
        """Overloaded traverse callback to handle variable search."""
        STATDotWindow.on_to_traverse_least_progress(self, action)
        self.check_for_loop_variable()

    def on_to_traverse_most_progress(self, action):
        """Overloaded traverse callback to handle variable search."""
        STATDotWindow.on_to_traverse_most_progress(self, action)
        self.check_for_loop_variable()

    def clear_var_spec_and_destroy_dialog(self, dialog):
        """Handle case where we don't want to gather loop variable."""
        self.var_spec = []
        dialog.destroy()

    def check_for_loop_variable(self):
        """Searches for loop ordering variables.

        If found, prompt user to gather stack trace with variable values."""
        found = False
        for node in self.get_current_graph().nodes:
            if node.lex_string != None:
                if node.lex_string.find('$') != -1 and node.lex_string.find('=') == -1:
                    function_name, sourceLine, iter_string = decompose_node(node.label)
                    source = sourceLine[:sourceLine.find(':')]
                    line = int(sourceLine[sourceLine.find(':') + 1:])
                    temp = node.lex_string[node.lex_string.find('$') + 1:]
                    while 1:
                        var = temp[:temp.find('(')]
                        if var == 'iter#':
                            break
                        found = True
                        self.var_spec.append((source, line, node.depth, var))
                        if temp.find('$') != -1:
                            temp = temp[temp.find('$') + 1:]
                        else:
                            break
        if found == True:
            dialog = gtk.Dialog('Gather Loop Order Variable(s)?', self)
            label = gtk.Label('Temporal ordering requires STAT\nto fetch loop ordering variables.\nDo you want STAT to gather these variables?\nNote, this generates a new call prefix tree.')
            dialog.vbox.pack_start(label, False, False, 0)
            hbox = gtk.HButtonBox()
            button = gtk.Button(stock = gtk.STOCK_YES)

            #button.connect("clicked", lambda w: self.run_and_destroy_dialog(stat_wait_dialog.show_wait_dialog_and_run, (self.sample, (True,), self), self.sample_task_list, dialog))
            button.connect("clicked", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.run_and_destroy_dialog, (self.hacky_sample_keep_state, (), dialog), self.sample_task_list))

            hbox.pack_start(button, True, True, 0)
            button = gtk.Button('Open in\nnew tab')

            #button.connect("clicked", lambda w: self.run_and_destroy_dialog(self.new_tab_and_sample, (), dialog))
            button.connect("clicked", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.run_and_destroy_dialog, (self.hacky_sample_keep_state, (True,), dialog), self.sample_task_list))

            hbox.pack_start(button, True, True, 0)
            button = gtk.Button(stock = gtk.STOCK_NO)
            button.connect("clicked", lambda w: self.clear_var_spec_and_destroy_dialog(dialog))
            hbox.pack_start(button, True, True, 0)
            dialog.vbox.pack_start(hbox, False, False, 0)
            dialog.show_all()
            dialog.run()

    def new_tab_and_sample(self):
        """Create a new tab and gather a stack sample"""
        page = self.notebook.get_current_page()
        self.create_new_tab(page + 1)
        self.notebook.set_current_page(page + 1)
        self.sample(True)

    def hacky_sample_keep_state(self, new_tab=False):
        """Gather a new stack trace.

        Also perform TO operations from previous trace.
        """
        actions = self.get_current_graph().action_history
        if new_tab == True:
            page = self.notebook.get_current_page()
            self.create_new_tab(page + 1)
            self.notebook.set_current_page(page + 1)
        self.sample(True)
        for action in actions:
            if action.find('Traverse Most Progress') != -1:
                depth = int(action[23:])
                self.on_to_traverse_most_progress(None)
            if action.find('Traverse Least Progress') != -1:
                depth = int(action[23:])
                self.on_to_traverse_least_progress(None)
        self.get_current_graph().adjust_dims()
        self.get_current_widget().zoom_to_fit()


    def run_and_destroy_dialog(self, function, args, dialog):
        """Run the specified function and destroys the specified dialog."""
        apply(function, args)
        dialog.destroy()

    def on_reattach(self, action):
        """Attach to the same job as the previous session.

        Also gather a stack trace.
        """
        if self.serial_attach == True:
            if self.STAT is None:
                self.STAT = STAT_FrontEnd()
            for process in self.process_list.split():
                self.STAT.addSerialProcess(process)
            self.attach_cb(None, False, True, STAT_SERIAL_ATTACH)
        else:
            self.attach_cb(None, False, False, STAT_ATTACH)
        return True

    def on_attach(self, action):
        """Generate a dialog to attach to a new job."""
        if self.reattach == True and self.STAT != None:
            self.STAT.shutDown()
            self.STAT = None
        if self.STAT is None:
            self.STAT = STAT_FrontEnd()
        self.options['PID'] = None
        self.options['Launcher Exe'] = ''
        self.options['Serial PID'] = None
        self.options['Serial Exe'] = ''
        attach_dialog = gtk.Dialog('Attach', self)
        attach_dialog.set_default_size(400, 400)
        notebook = gtk.Notebook()
        notebook.set_tab_pos(gtk.POS_TOP)
        notebook.set_scrollable(False)

        # parallel attach
        process_frame = gtk.Frame('Current Process List')
        vbox = gtk.VBox()
        hbox = gtk.HBox()
        self.pack_entry_and_button(self.options['Remote Host'], self.on_update_remote_host, process_frame, attach_dialog, "Search Remote Host", hbox)
        hbox.pack_start(gtk.VSeparator(), False, False, 5)
        self.pack_combo_box(hbox, 'Remote Host Shell')
        vbox.pack_start(hbox, False, False, 0)
        hbox = gtk.HBox()
        hbox.pack_start(gtk.Label('Filter Process List'), False, False, 5)
        self.filter_entry = self.pack_entry_and_button(self.options['Job Launcher'], self.on_update_process_listing, process_frame, attach_dialog, "Filter", hbox, True, True, 0)
        vbox.pack_start(hbox, False, False, 0)
        vbox.pack_start(process_frame, True, True, 0)
        hbox = gtk.HBox()
        button = gtk.Button("Refresh Process List")
        button.connect("clicked", lambda w: self.on_update_process_listing(w, process_frame, attach_dialog, self.filter_entry))
        hbox.pack_start(button, True, True, 10)
        vbox.pack_start(hbox, False, False, 0)
        vbox.pack_start(gtk.HSeparator(), False, False, 10)
        hbox = gtk.HBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_start(button, False, False, 0)
        button = gtk.Button("Attach")
        button.connect("clicked", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.attach_cb, (attach_dialog, False, False, STAT_ATTACH), self.attach_task_list, attach_dialog))
        hbox.pack_start(button, True, True, 0)
        vbox.pack_start(hbox, False, False, 0)
        label = gtk.Label('Attach')
        notebook.append_page(vbox, label)

        # launch
        vbox = gtk.VBox()
        hbox=gtk.HBox()
        hbox.pack_start(gtk.Label('Enter application launch string:'), False, False, 0)
        vbox.pack_start(hbox, False, False, 10)
        entry = gtk.Entry()
        entry.set_max_length(8192)
        entry.connect("activate", lambda w: self.launch_application_cb(entry, attach_dialog))
        vbox.pack_start(entry, False, False, 0)
        hbox = gtk.HButtonBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_end(button, False, False, 0)
        button = gtk.Button("Launch")
        button.connect("clicked", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.launch_application_cb, (entry, attach_dialog), self.attach_task_list, attach_dialog))
        hbox.pack_start(button, False, False, 0)
        vbox.pack_end(hbox, False, False, 0)
        label = gtk.Label('Launch')
        notebook.append_page(vbox, label)

        # serial attach tab
        serial_process_frame = gtk.Frame('Current Process List')
        vbox = gtk.VBox()
        hbox = gtk.HBox()
        hbox.pack_start(gtk.Label('Search Host'), False, False, 5)
        self.pack_entry_and_button(self.options['Remote Host'], self.on_update_serial_remote_host, serial_process_frame, attach_dialog, "Search", hbox)
        hbox.pack_start(gtk.VSeparator(), False, False, 5)
        self.pack_combo_box(hbox, 'Remote Host Shell')
        vbox.pack_start(hbox, False, False, 0)
        hbox = gtk.HBox()
        hbox.pack_start(gtk.Label('Filter Process List'), False, False, 5)
        self.serial_filter_entry = self.pack_entry_and_button("", self.on_update_serial_process_listing, serial_process_frame, attach_dialog, "Filter", hbox, True, True, 0)
        vbox.pack_start(hbox, False, False, 0)
        vbox.pack_start(serial_process_frame, True, True, 0)
        hbox = gtk.HBox()
        button = gtk.Button("Refresh Process List")
        button.connect("clicked", lambda w: self.on_update_serial_process_listing(w, serial_process_frame, attach_dialog, self.serial_filter_entry))
        hbox.pack_start(button, False, False, 10)
        button = gtk.Button("Add Process to Attach List")
        hbox.pack_start(button, True, True, 10)
        self.serial_process_list_entry = gtk.Entry()
        self.serial_process_list_entry.set_max_length(1024)
        self.serial_process_list_entry.set_text(self.options['Serial Process List'])
        button.connect("clicked", lambda w: self.on_add_serial_process(w, attach_dialog, self.serial_process_list_entry))
        vbox.pack_start(hbox, False, False, 10)
        hbox = gtk.HBox()
        label = gtk.Label("Attach List")
        hbox.pack_start(label, False, False, 10)
        self.serial_process_list_entry.connect("activate", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.attach_serial_processes_cb, (self.serial_process_list_entry, attach_dialog), self.attach_task_list, attach_dialog))
        hbox.pack_start(self.serial_process_list_entry, True, True, 0)
        vbox.pack_start(hbox, False, False, 0)
        vbox.pack_start(gtk.HSeparator(), False, False, 10)
        hbox = gtk.HBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_start(button, False, False, 10)
        button = gtk.Button("Attach to Attach List")
        button.connect("clicked", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.attach_serial_processes_cb, (self.serial_process_list_entry, attach_dialog), self.attach_task_list, attach_dialog))
        hbox.pack_start(button, True, True, 0)
        vbox.pack_start(hbox, False, False, 0)
        label = gtk.Label('Serial Attach')
        notebook.append_page(vbox, label)

        # sample options
        vbox = gtk.VBox()
        self.pack_sample_options(vbox, False, True)
        hbox = gtk.HBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_start(button, False, False, 0)
        vbox.pack_end(hbox, False, False, 0)
        label = gtk.Label('Sample Options')
        notebook.append_page(vbox, label)

        # topology options
        vbox = gtk.VBox()
        frame = gtk.Frame('Topology Options')
        vbox2 = gtk.VBox()
        self.pack_combo_box(vbox2, 'Topology Type')
        self.pack_string_option(vbox2, 'Topology', attach_dialog)
        self.pack_string_option(vbox2, 'Communication Nodes', attach_dialog)
        self.pack_check_button(vbox2, 'Share App Nodes')
        self.pack_spinbutton(vbox2, 'Communication Processes per Node')
        frame.add(vbox2)
        vbox.pack_start(frame, False, False, 0)
        hbox = gtk.HBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_start(button, False, False, 0)
        vbox.pack_end(hbox, False, False, 0)
        label = gtk.Label('Topology')
        notebook.append_page(vbox, label)

        # misc options
        vbox = gtk.VBox()
        frame = gtk.Frame('Tool Paths')
        vbox2 = gtk.VBox()
        self.pack_string_option(vbox2, 'Tool Daemon Path', attach_dialog)
        self.pack_string_option(vbox2, 'Filter Path', attach_dialog)
        frame.add(vbox2)
        vbox.pack_start(frame, False, False, 5)
        frame = gtk.Frame('Debug Logs')
        vbox2 = gtk.VBox()
        self.pack_check_button(vbox2, 'Log Frontend')
        self.pack_check_button(vbox2, 'Log Backend')
        self.pack_check_button(vbox2, 'Log CP')
        self.pack_check_button(vbox2, 'Log SW')
        self.pack_check_button(vbox2, 'Log SWERR')
        self.pack_string_option(vbox2, 'Log Dir', attach_dialog)
        self.pack_check_button(vbox2, 'Use MRNet Printf')
        frame.add(vbox2)
        vbox.pack_start(frame, False, False, 5)
        frame = gtk.Frame('Misc')
        vbox2 = gtk.VBox()
        self.pack_combo_box(vbox2, 'Verbosity Type')
        self.pack_check_button(vbox2, 'Debug Backends')
        frame.add(vbox2)
        vbox.pack_start(frame, False, False, 5)
        hbox = gtk.HBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_start(button, False, False, 0)
        vbox.pack_end(hbox, False, False, 0)
        label = gtk.Label('Advanced')
        notebook.append_page(vbox, label)

        self.on_update_process_listing(None, process_frame, attach_dialog, self.filter_entry)
        self.on_update_serial_process_listing(None, serial_process_frame, attach_dialog, self.serial_filter_entry)
        attach_dialog.vbox.pack_start(notebook, True, True, 0)
        attach_dialog.show_all()
        attach_dialog.run()
        self.sw = None
        self.serial_sw = None

    def attach_serial_processes_cb(self, entry, attach_dialog):
        """Callback to attach to serial processes."""
        processes = entry.get_text()
        self.serial_attach = True
        self.process_list = processes
        processes = processes.split()
        if len(processes) <= 0:
            show_error_dialog('No processes selected.  Please select processes\nto attach to by selecting the desired radio\nbuttons and clicking "Add Serial Process" or by\nmanually entering host:pid pairs in the text entry.', self)
            return False
        if self.STAT is None:
            self.STAT = STAT_FrontEnd()
        for process in processes:
            self.STAT.addSerialProcess(process)
        self.attach_cb(attach_dialog, False, True, STAT_SERIAL_ATTACH)

    def launch_application_cb(self, entry, attach_dialog):
        """Callback to launch an application under STAT's control."""
        args = entry.get_text()
        args = args.split()
        if len(args) <= 0:
            show_error_dialog('No launch string specified.  Please enter\nthe application launch string.\n', self)
            return False
        self.options['Launcher Exe'] = args[0]
        for arg in args:
            self.STAT.addLauncherArgv(arg)
        self.attach_cb(attach_dialog, True, False, STAT_LAUNCH)

    def attach_cb(self, attach_dialog, launch, serial, application_option=STAT_ATTACH):
        """Callback to attach to a job and gather a stack trace."""
        if serial == False:
            self.serial_attach = False
            self.process_list = None
        else:
            self.serial_attach = True
        stat_wait_dialog.update_progress_bar(0.01)
        if attach_dialog != None:
            attach_dialog.destroy()
        self.options['Topology Type'] = self.types['Topology Type'][self.combo_boxes['Topology Type'].get_active()]
        self.options['Verbosity Type'] = self.types['Verbosity Type'][self.combo_boxes['Verbosity Type'].get_active()]
        self.options['Communication Processes per Node'] = int(self.spinners['Communication Processes per Node'].get_value())
        if self.STAT is None:
            self.STAT = STAT_FrontEnd()
        if launch == False and serial == False:
            if self.options['PID'] is None:
                show_error_dialog('No job selected.  Please select a valid\njob launcher process to attach to.\n', self)
                return False

        self.STAT.setToolDaemonExe(self.options['Tool Daemon Path'])
        self.STAT.setFilterPath(self.options['Filter Path'])
        logType = STAT_LOG_NONE
        if self.options['Log Frontend']:
            logType |= STAT_LOG_FE
        if self.options['Log Backend']:
            logType |= STAT_LOG_BE
        if self.options['Log CP']:
            logType |= STAT_LOG_CP
        if self.options['Log SW']:
            logType |= STAT_LOG_SW
        if self.options['Log SWERR']:
            logType |= STAT_LOG_SWERR
        if self.options['Use MRNet Printf']:
            logType |= STAT_LOG_MRN
        if logType != STAT_LOG_NONE:
            ret = self.STAT.startLog(logType, self.options['Log Dir'])
            if ret != STAT_OK:
                show_error_dialog('Failed to Start Log:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return False
        if self.options['Verbosity Type'] == 'error':
            self.STAT.setVerbose(STAT_VERBOSE_ERROR)
        elif self.options['Verbosity Type'] == 'stdout':
            self.STAT.setVerbose(STAT_VERBOSE_STDOUT)
        elif self.options['Verbosity Type'] == 'full':
            self.STAT.setVerbose(STAT_VERBOSE_FULL)
        if self.options['Debug Backends']:
            os.environ['LMON_DEBUG_BES'] = "1"
        else:
            if 'LMON_DEBUG_BES' in os.environ:
                del os.environ['LMON_DEBUG_BES']
        self.STAT.setProcsPerNode(self.options['Communication Processes per Node'])
        stat_wait_dialog.update_progress_bar(0.05)

        self.STAT.setApplicationOption(application_option)
        if launch == False:
            if serial == False:
                if self.options['Remote Host'] != "localhost":
                    ret = self.STAT.attachAndSpawnDaemons(self.options['PID'], self.options['Remote Host'])
                else:
                    ret = self.STAT.attachAndSpawnDaemons(self.options['PID'])
            else:
                ret = self.STAT.setupForSerialAttach()
        else:
            if self.options['Remote Host'] != "localhost":
                ret = self.STAT.launchAndSpawnDaemons(self.options['Remote Host'])
            else:
                ret = self.STAT.launchAndSpawnDaemons()
            if self.options['PID'] is None:
                self.set_proctab()
                self.options['PID'] = self.proctab.launcher_pid
        if 'LMON_DEBUG_BES' in os.environ:
            del os.environ['LMON_DEBUG_BES']
        if ret != STAT_OK:
            show_error_dialog('Failed to Launch Daemons:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return False
        stat_wait_dialog.update(0.10)

        topology_type = STAT_TOPOLOGY_AUTO
        if self.options['Topology Type'] == 'automatic':
            topology_type = STAT_TOPOLOGY_AUTO
        elif self.options['Topology Type'] == 'depth':
            topology_type = STAT_TOPOLOGY_DEPTH
        elif self.options['Topology Type'] == 'max fanout':
            topology_type = STAT_TOPOLOGY_FANOUT
        elif self.options['Topology Type'] == 'custom':
            topology_type = STAT_TOPOLOGY_USER
        if self.options['Communication Nodes'] != '':
            ret = self.STAT.launchMrnetTree(topology_type, self.options['Topology'], self.options['Communication Nodes'], False, self.options['Share App Nodes'])
        else:
            ret = self.STAT.launchMrnetTree(topology_type, self.options['Topology'], '', False, self.options['Share App Nodes'])
        if ret != STAT_OK:
            show_error_dialog('Failed to Launch MRNet Tree:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return False

        if application_option != STAT_SERIAL_ATTACH:
            while 1:
                if gtk.events_pending():
                    gtk.main_iteration()
                ret = self.STAT.connectMrnetTree(False)
                if ret == STAT_OK:
                    break
                elif ret != STAT_PENDING_ACK:
                    show_error_dialog('Failed to connect to MRNet tree:\n%s' %self.STAT.getLastErrorMessage(), self)
                    self.on_fatal_error()
                    return ret
        ret = self.STAT.setupConnectedMrnetTree();
        if ret != STAT_OK:
            show_error_dialog('Failed to setup STAT:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return False

        stat_wait_dialog.update(0.15)
        ret = self.STAT.attachApplication(False)
        if ret != STAT_OK:
            show_error_dialog('Failed to attach to application:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return False
        while 1:
            if gtk.events_pending():
                gtk.main_iteration()
            ret = self.STAT.receiveAck(False)
            if ret == STAT_OK:
                break
            elif ret != STAT_PENDING_ACK:
                show_error_dialog('Failed to attach:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
        stat_wait_dialog.update(0.20)
        self.attached = True
        ret = self.sample()
        if ret != STAT_OK:
            return ret
        self.set_action_sensitivity('paused')
        stat_wait_dialog.update_progress_bar(1.0)

    def on_detach(self, widget, stop_list = intArray(0), stop_list_len=0):
        """Determine the process state and detach from the current job."""
        self.proctab_file_path = None
        self.proctab = None
        if self.attached == False:
            self.STAT = None
            self.reattach = False
            self.proctab_file_path = None
            self.proctab = None
            return True
        self.var_spec = []
        self.show_all()

        ret = self.STAT.detachApplication(stop_list, stop_list_len, False)
        if ret != STAT_OK:
            show_error_dialog('Failed to detach from application:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret
        while 1:
            if gtk.events_pending():
                gtk.main_iteration()
            ret = self.STAT.receiveAck(False)
            if ret == STAT_OK:
                break
            elif ret != STAT_PENDING_ACK:
                show_error_dialog('Failed to sample stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
        if ret != STAT_OK:
            show_error_dialog('Failed to resume application:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return False
        self.STAT.shutDown()
        self.STAT = None
        self.reattach = True
        self.attached = False
        self.set_action_sensitivity('detached')

    def on_destroy(self, action):
        """Callback to quit."""
        if self.STAT != None:
            if self.attached:
                ret = self.STAT.detachApplication(True)
                if ret != STAT_OK:
                    sys.stderr.write('Failed to detach from application:\n%s\n' %self.STAT.getLastErrorMessage())
                self.STAT.shutDown()
            self.STAT = None
        gtk.main_quit()

    def on_pause(self, action=None):
        """Callback to pause the job."""
        self.set_action_sensitivity('paused')
        ret = self.STAT.pause()
        if ret != STAT_OK:
            show_error_dialog('Failed to pause application:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret
        while 1:
            if gtk.events_pending():
                gtk.main_iteration()
            ret = self.STAT.receiveAck(False)
            if ret == STAT_OK:
                break
            elif ret != STAT_PENDING_ACK:
                show_error_dialog('Failed to pause application:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
        return ret

    def on_resume(self, action=None):
        """Callback to resume the job."""
        self.set_action_sensitivity('running')
        ret = self.STAT.resume()
        if ret != STAT_OK:
            show_error_dialog('Failed to resume application:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret
        while 1:
            if gtk.events_pending():
                gtk.main_iteration()
            ret = self.STAT.receiveAck(False)
            if ret == STAT_OK:
                break
            elif ret != STAT_PENDING_ACK:
                show_error_dialog('Failed to resume application:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
        return ret

    def on_kill(self, action):
        """Callback to kill the job."""
        self.set_action_sensitivity('new')

    def update_sample_options(self):
        """Update sample options."""
        try:
            self.options['Num Traces'] = int(self.spinners['Num Traces'].get_value())
            self.options['Trace Frequency (ms)'] = int(self.spinners['Trace Frequency (ms)'].get_value())
        except:
            pass
        self.options['Num Retries'] = int(self.spinners['Num Retries'].get_value())
        self.options['Retry Frequency (us)'] = int(self.spinners['Retry Frequency (us)'].get_value())
        try:
            self.options['Run Time Before Sample (sec)'] = int(self.spinners['Run Time Before Sample (sec)'].get_value())
        except:
            pass

    def sleep(self, seconds):
        """Sleep for specified time with a progress bar."""
        for i in xrange(int(seconds * 100)):
            stat_wait_dialog.update_progress_bar(float(i) / seconds / 100)
            time.sleep(.01)

    def update_prefs_and_sample_cb(self, sample_function, widget, dialog, action):
        """Callback to update sample preferences and sample traces."""
        dialog.destroy()
        self.update_sample_options()
        if self.options['Run Time Before Sample (sec)'] != 0:
            if self.STAT.isRunning() == False:
                ret = self.on_resume()
                if ret != STAT_OK:
                    return False
            stat_wait_dialog.show_wait_dialog_and_run(self.sleep, (self.options['Run Time Before Sample (sec)'],), [], self)
        if sample_function == self.sample_multiple:
            stat_wait_dialog.show_wait_dialog_and_run(sample_function, (), self.sample_task_list, self, True)
        else:
            stat_wait_dialog.show_wait_dialog_and_run(sample_function, (), self.sample_task_list, self)

    def on_sample(self, action, multiple):
        """Callback to sample stack traces."""
        dialog = gtk.Dialog('Stack Sample Preferences', self)
        vbox = gtk.VBox()
        self.pack_sample_options(vbox, multiple)
        dialog.vbox.pack_start(vbox, False, False, 0)
        hbox = gtk.HButtonBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: dialog.destroy())
        hbox.pack_start(button, False, True, 0)
        button = gtk.Button(stock = gtk.STOCK_OK)
        if multiple == True:
            button.connect("clicked", lambda w: self.update_prefs_and_sample_cb(self.sample_multiple, w, dialog, 'OK'))
        else:
            button.connect("clicked", lambda w: self.update_prefs_and_sample_cb(self.sample, w, dialog, 'OK'))
        hbox.pack_start(button, False, True, 0)
        dialog.vbox.pack_start(hbox, False, False, 0)
        dialog.show_all()
        dialog.run()

    def sample(self, keep_var_spec=False):
        """Sample stack traces."""
        ret_val = STAT_OK
        if keep_var_spec == False:
            self.var_spec = []
            # below is a test for local variables:
            #self.var_spec.append(("/g/g0/lee218/src/STAT/examples/src/to_test.c", 38, 5, "i"))
            #self.var_spec.append(("/g/g0/lee218/src/STAT/examples/src/to_test.c", 38, 4, "i"))
        self.update_sample_options()
        ret = self.STAT.pause()
        stat_wait_dialog.update_progress_bar(0.35)
        self.set_action_sensitivity('paused')
        if ret != STAT_OK:
            show_error_dialog('Failed to pause application during sample stack trace operation:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret

        sample_type = 0
        if self.options['Sample Type'] == 'function and pc':
            sample_type += STAT_SAMPLE_PC
        if self.options['Sample Type'] == 'function and line':
            sample_type += STAT_SAMPLE_LINE
        if self.options['Edge Type'] != 'full list':
            sample_type += STAT_SAMPLE_COUNT_REP
        if self.options['With Threads']:
            sample_type += STAT_SAMPLE_THREADS
        if self.options['Gather Python Traces']:
            sample_type += STAT_SAMPLE_PYTHON
        if self.options['Clear On Sample']:
            sample_type += STAT_SAMPLE_CLEAR_ON_SAMPLE
        ret = self.STAT.sampleStackTraces(sample_type, 1, 1, self.options['Num Retries'], self.options['Retry Frequency (us)'], False, var_spec_to_string(self.var_spec))

        if ret != STAT_OK:
            show_error_dialog('Failed to sample stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret
        while 1:
            if gtk.events_pending():
                gtk.main_iteration()
            ret = self.STAT.receiveAck(False)
            if ret == STAT_OK:
                break
            elif ret != STAT_PENDING_ACK:
                show_error_dialog('Failed to sample stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
        stat_wait_dialog.update(0.60)
        ret = self.STAT.gatherLastTrace(False)
        if ret != STAT_OK:
            show_error_dialog('Failed to gather stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret
        while 1:
            if gtk.events_pending():
                gtk.main_iteration()
            ret = self.STAT.receiveAck(False)
            if ret == STAT_OK:
                break
            elif ret != STAT_PENDING_ACK:
                show_error_dialog('Failed to sample stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
        stat_wait_dialog.update(0.70)
        filename = self.STAT.getLastDotFilename()
        try:
            with open(filename, 'r') as f:
                self.tabs[self.notebook.get_current_page()].history_view.get_buffer().set_text('')
                self.set_dotcode(f.read(), filename, self.notebook.get_current_page())
        except IOError as e:
            show_error_dialog('%s\nFailed to open file %s' %(repr(e), filename), self)
            return ret
        except Exception as e:
            show_error_dialog('%s\nFailed to process file %s' %(repr(e), filename), self)
            return ret
        stat_wait_dialog.update(1.0)
        if ret_val != STAT_OK:
            show_error_dialog('An error was detected:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
        return ret_val

    def sample_multiple(self):
        """Sample multiple stack traces."""

        ret_val = STAT_OK
        self.update_sample_options()
        stat_wait_dialog.update_progress_bar(0.01)

        previous_clear = self.options['Clear On Sample']
        for i in xrange(self.options['Num Traces']):
            if stat_wait_dialog.cancelled == True:
                stat_wait_dialog.cancelled = False
                break
            if i == 1:
                self.options['Clear On Sample'] = False
            ret = self.STAT.pause()
            if ret == STAT_APPLICATION_EXITED:
                ret_val = STAT_APPLICATION_EXITED
                break
            if ret != STAT_OK:
                show_error_dialog('Failed to pause application during sample multiple operation:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
            self.set_action_sensitivity('paused')

            sample_type = 0
            if self.options['Sample Type'] == 'function and pc':
                sample_type += STAT_SAMPLE_PC
            if self.options['Sample Type'] == 'function and line':
                sample_type += STAT_SAMPLE_LINE
            if self.options['Edge Type'] != 'full list':
                sample_type += STAT_SAMPLE_COUNT_REP
            if self.options['With Threads']:
                sample_type += STAT_SAMPLE_THREADS
            if self.options['Gather Python Traces']:
                sample_type += STAT_SAMPLE_PYTHON
            if self.options['Clear On Sample']:
                sample_type += STAT_SAMPLE_CLEAR_ON_SAMPLE
            ret = self.STAT.sampleStackTraces(sample_type, 1, 0, self.options['Num Retries'], self.options['Retry Frequency (us)'], False, var_spec_to_string(self.var_spec))
            if ret != STAT_OK:
                if ret == STAT_APPLICATION_EXITED:
                    ret_val = STAT_APPLICATION_EXITED
                    break
                show_error_dialog('Failed to sample stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
            while 1:
                if gtk.events_pending():
                    gtk.main_iteration()
                ret = self.STAT.receiveAck(False)
                if ret == STAT_OK:
                    break
                elif ret == STAT_APPLICATION_EXITED:
                    ret_val = STAT_APPLICATION_EXITED
                    break
                elif ret != STAT_PENDING_ACK:
                    show_error_dialog('Failed to receive ack from sample operation:\n%s' %self.STAT.getLastErrorMessage(), self)
                    self.on_fatal_error()
                    return ret
            if ret == STAT_APPLICATION_EXITED:
                ret_val = STAT_APPLICATION_EXITED
                break

            if self.options['Gather Individual Samples'] == True:
                ret = self.STAT.gatherLastTrace(False)
                if ret != STAT_OK:
                    show_error_dialog('Failed to gather last stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
                    self.on_fatal_error()
                    return ret
                while 1:
                    if gtk.events_pending():
                        gtk.main_iteration()
                    ret = self.STAT.receiveAck(False)
                    if ret == STAT_OK:
                        break
                    elif ret != STAT_PENDING_ACK:
                        show_error_dialog('Failed to receive last stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
                        self.on_fatal_error()
                        return ret
                filename = self.STAT.getLastDotFilename()
                try:
                    with open(filename, 'r') as f:
                        page = self.notebook.get_current_page()
                        self.create_new_tab(page + 1)
                        self.notebook.set_current_page(page + 1)
                        self.tabs[self.notebook.get_current_page()].history_view.get_buffer().set_text('')
                        self.set_dotcode(f.read(), filename, self.notebook.get_current_page())
                except IOError as e:
                    show_error_dialog('%s\nFailed to open file %s' %(repr(e), filename), self)
                    return False
                except Exception as e:
                    show_error_dialog('%s\nFailed to process file %s' %(repr(e), filename), self)
                    return False

            stat_wait_dialog.update_progress_bar(float(i) / self.options['Num Traces'])
            if ret_val != STAT_OK:
                break
            if i != self.options['Num Traces'] -1:
                ret = self.STAT.resume()
                if ret == STAT_APPLICATION_EXITED:
                    ret_val = STAT_APPLICATION_EXITED
                    break
                elif ret != STAT_OK:
                    return ret
                self.set_action_sensitivity('running')
                for i in xrange(int(self.options['Trace Frequency (ms)'] / 10)):
                    time.sleep(.01)
                    if gtk.events_pending():
                        gtk.main_iteration()
        self.options['Clear On Sample'] = previous_clear
        stat_wait_dialog.update(0.91)
        ret = self.STAT.gatherTraces(False)
        if ret != STAT_OK:
            show_error_dialog('Failed to gather stack traces:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret
        while 1:
            if gtk.events_pending():
                gtk.main_iteration()
            ret = self.STAT.receiveAck(False)
            if ret == STAT_OK:
                break
            elif ret != STAT_PENDING_ACK:
                show_error_dialog('Failed to gather stack traces:\n%s' %self.STAT.getLastErrorMessage(), self)
                self.on_fatal_error()
                return ret
        stat_wait_dialog.update(0.95)
        filename = self.STAT.getLastDotFilename()
        try:
            with open(filename, 'r') as f:
                if self.options['Gather Individual Samples'] == True:
                    page = self.notebook.get_current_page()
                    self.create_new_tab(page + 1)
                    self.notebook.set_current_page(page + 1)
                else:
                    self.tabs[self.notebook.get_current_page()].history_view.get_buffer().set_text('')
                self.set_dotcode(f.read(), filename, self.notebook.get_current_page())
        except IOError as e:
            show_error_dialog('%s\nFailed to open file %s' %(repr(e), filename), self)
            return False
        except Exception as e:
            show_error_dialog('%s\nFailed to process file %s' %(repr(e), filename), self)
            return False
        stat_wait_dialog.update(1.0)
        if ret_val != STAT_OK:
            show_error_dialog('An error was detected:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()

        return ret_val

    def on_identify_num_eq_classes(self, action):
        """Callback to identify equivalence classes."""
        if self.get_current_graph().cur_filename == '':
            return False
        num_eq_classes = self.get_current_graph().identify_num_eq_classes(self.get_current_widget())
        self.eq_state = {}
        self.dont_recurse = False
        eq_dialog = gtk.Dialog("Equivalence Classes", self)
        eq_dialog.set_default_size(400, 400)
        my_frame = gtk.Frame("%d Equivalence Classes:" %(len(num_eq_classes)))
        sw = gtk.ScrolledWindow()
        sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        eq_string = ''
        classes = []
        for eq_class in num_eq_classes:
            current_class = '['
            prev = -2
            in_range = False
            task_count = 0
            task_list, fill_color_string, font_color_string = eq_class
            for num in task_list:
                task_count += 1
                if num == prev + 1:
                    in_range = True
                else:
                    if in_range == True:
                        current_class += '-%d'%prev
                    if prev != -2:
                        current_class += ', '
                    current_class += '%d'%num
                    in_range = False
                prev = num
            if in_range == True:
                current_class += '-%d'%num
            current_class += ']\n'
            if task_count == 1:
                eq_string += '%d task: %s' %(task_count, current_class)
            else:
                eq_string += '%d tasks: %s' %(task_count, current_class)
            item = {}
            item['class'] = task_list
            item['fill_color_string'] = fill_color_string
            item['font_color_string'] = font_color_string
            item['string'] = current_class.strip('\n').replace(',', ', ')
            classes.append(item)
        hbox = gtk.HBox()
        label = gtk.Label(' Rep  All  None  tasks')
        hbox.pack_start(label, False, False, 0)
        vbox = gtk.VBox()
        vbox.pack_start(hbox, False, False, 0)
        hbox = gtk.HBox()
        rep_button = gtk.CheckButton()
        all_button = gtk.CheckButton()
        none_button = gtk.CheckButton()
        rep_button.set_active(True)
        hbox.pack_start(rep_button, False, False, 0)
        hbox.pack_start(all_button, False, False, 7)
        hbox.pack_start(none_button, False, False, 9)
        label = gtk.Label(' select all')
        hbox.pack_start(label, False, False, 0)
        vbox.pack_start(hbox, False, False, 0)
        self.eq_state['rep_button'] = rep_button
        self.eq_state['all_button'] = all_button
        self.eq_state['none_button'] = none_button
        rep_button.connect('toggled', lambda w: self.on_toggle_eq(w, 'all', 'rep_button'))
        all_button.connect('toggled', lambda w: self.on_toggle_eq(w, 'all', 'all_button'))
        none_button.connect('toggled', lambda w: self.on_toggle_eq(w, 'all', 'none_button'))
        for item in classes:
            hbox = gtk.HBox()
            rep_button = gtk.RadioButton(None)
            all_button = gtk.RadioButton(rep_button)
            none_button = gtk.RadioButton(rep_button)
            rep_button.connect('toggled', lambda w: self.on_toggle_eq(w, classes.index(item), 'rep_button'))
            all_button.connect('toggled', lambda w: self.on_toggle_eq(w, classes.index(item), 'all_button'))
            none_button.connect('toggled', lambda w: self.on_toggle_eq(w, classes.index(item), 'none_button'))
            rep_button.set_active(True)
            item['rep_button'] = rep_button
            item['all_button'] = all_button
            item['none_button'] = none_button
            hbox.pack_start(rep_button, False, False, 0)
            hbox.pack_start(all_button, False, False, 7)
            hbox.pack_start(none_button, False, False, 9)
            list_view = gtk.TextView()
            list_view.set_wrap_mode(True)
            list_view.set_editable(False)
            list_view.set_cursor_visible(False)
            iter = list_view.get_buffer().get_iter_at_offset(0)
            list_view.get_buffer().create_tag("monospace", family = "monospace")
            foreground = gtk.gdk.color_parse(item['font_color_string'])
            background = gtk.gdk.color_parse(item['fill_color_string'])
            fore_color_tag = 'fore%s%s' %(item['font_color_string'], item['fill_color_string'])
            back_color_tag = 'back%s%s' %(item['font_color_string'], item['fill_color_string'])
            list_view.get_buffer().create_tag(fore_color_tag, foreground_gdk = foreground)
            list_view.get_buffer().create_tag(back_color_tag, background_gdk = background)
            list_view.get_buffer().insert_with_tags_by_name(iter, item['string'], fore_color_tag, back_color_tag, "monospace")
            hbox.pack_start(list_view, True, True, 5)
            vbox.pack_start(gtk.HSeparator(), False, False, 0)
            vbox.pack_start(hbox, False, False, 0)
        self.eq_state['classes'] = classes
        sw.add_with_viewport(vbox)
        my_frame.add(sw)
        eq_dialog.vbox.pack_start(my_frame, True, True, 5)
        my_frame = gtk.Frame("Manually Specify Additional Tasks:")
        entry = gtk.Entry()
        entry.set_max_length(65536)
        my_frame.add(entry)
        self.eq_state['entry'] = entry
        eq_dialog.vbox.pack_start(my_frame, False, False, 5)
        self.separator = gtk.HSeparator()
        eq_dialog.vbox.pack_start(self.separator, False, True, 5)
        box2 = gtk.HButtonBox()

        debuggers = ['TotalView', 'DDT']
        for debugger in debuggers:
            button = gtk.Button(" Attach %s \n to Subset " %debugger)
            button.connect("clicked", self.launch_debugger_cb, (debugger, eq_dialog))
            box2.pack_start(button, False, True, 5)
        button = gtk.Button(" Debugger \n Options ")
        button.connect("clicked", self.set_debugger_options_cb, eq_dialog)
        box2.pack_start(button, False, True, 5)
        button = gtk.Button(stock=gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w, d: eq_dialog.destroy(), "ok")
        box2.pack_end(button, False, True, 5)
        eq_dialog.vbox.pack_end(box2, False, False, 0)
        eq_dialog.show_all()
        return True

    def set_debugger_options_cb(self, w, parent):
        """Callback to update debugger options."""
        dialog = gtk.Dialog('Debugger Options', parent)
        frame = gtk.Frame('Options')
        vbox = gtk.VBox()
        self.pack_string_option(vbox, 'DDT Path', dialog)
        self.pack_string_option(vbox, 'DDT LaunchMON Prefix', dialog)
        self.pack_string_option(vbox, 'TotalView Path', dialog)
        self.pack_string_option(vbox, 'Additional Debugger Args', dialog)
        frame.add(vbox)
        dialog.vbox.pack_start(frame, True, True, 0)
        hbox = gtk.HButtonBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: dialog.destroy())
        hbox.pack_start(button, True, True, 0)
        button = gtk.Button(stock = gtk.STOCK_OK)
        button.connect("clicked", lambda w: dialog.destroy())
        hbox.pack_start(button, True, True, 0)
        dialog.vbox.pack_start(hbox, False, False, 10)
        dialog.show_all()
        dialog.run()

    def on_toggle_eq(self, w, button, type):
        """Callback to toggle equivalence class representatives."""
        if self.dont_recurse == True:
            return True
        self.dont_recurse = True
        self.eq_state['rep_button'].set_active(False)
        self.eq_state['all_button'].set_active(False)
        self.eq_state['none_button'].set_active(False)
        if button == 'all':
            for item in self.eq_state['classes']:
                item[type].set_active(True)
            self.eq_state[type].set_active(True)
        self.dont_recurse = False
        return True

    def set_proctab(self):
        if self.proctab_file_path != None and self.proctab != None:
            return True

        self.proctab_file_path = ''
        try:
            out_dir = self.STAT.getOutDir()
            file_prefix = self.STAT.getFilePrefix()
            self.proctab_file_path = out_dir + '/' + file_prefix + '.ptab'
        except:
            pass
        if not os.path.exists(self.proctab_file_path):
            directory = os.path.dirname(os.path.abspath(self.get_current_graph().cur_filename))
            self.proctab_file_path = ''
            for file in os.listdir(directory):
                if file.find('.ptab') != -1:
                    self.proctab_file_path = directory + '/' + file
                    break
        if self.proctab_file_path == '':
            show_error_dialog('Failed to find process table .ptab file.', self)
            return False
        print 'ptab file', self.proctab_file_path

        try:
            with open(self.proctab_file_path, 'r') as f:
                launcher = f.next().strip('\n').split(':')
                self.proctab = ProcTab()
                self.proctab.launcher_host = launcher[0]
                self.proctab.launcher_pid = int(launcher[1])
                for line in f:
                    line = line.strip('\n').split()
                    rank = int(line[0])
                    host_pid = line[1].split(':')
                    host = host_pid[0]
                    pid = int(host_pid[1])
                    exe = line[2]
                    if exe in self.proctab.executable_paths:
                        index = self.proctab.executable_paths.index(exe)
                    else:
                        index = len(self.proctab.executable_paths)
                        self.proctab.executable_paths.append(exe)
                    if self.proctab.executable_path is None:
                        self.proctab.executable_path = exe
                    self.proctab.process_list.append((rank, host, pid, index))
        except IOError as e:
            show_error_dialog('%s\nfailed to open process table file:\n\n%s\n\nPlease be sure that it is a valid process table file outputted from STAT.' %(repr(e), self.proctab_file_path), self)
            return False
        except Exception as e:
            show_error_dialog('%s\nfailed to process process table file:\n\n%s\n\nPlease be sure that it is a valid process table file outputted from STAT.' %(repr(e), self.proctab_file_path), self)
            return False
        return True


    def launch_debugger_cb(self, widget, args):
        """Callback to launch full-featured debugger on a subset of tasks."""
        debugger, eq_dialog = args
        subset_list = []
        for item in self.eq_state['classes']:
            if item['rep_button'].get_active() == True and len(item['class']) > 0:
                subset_list.append(item['class'][0])
            elif item['all_button'].get_active() == True and len(item['class']) > 0:
                subset_list += item['class']
        additional_tasks = '[' + self.eq_state['entry'].get_text().strip(' ').strip('[').strip(']') + ']'
        task_list_set = set(get_task_list(additional_tasks) + subset_list)
        subset_list = list(task_list_set)
        if subset_list == []:
            show_error_dialog('No tasks selected.  Please choose a subset of tasks to debug', self)
            return False
        eq_dialog.destroy()
        # use current STAT session to determine job launcher PID
        ret = self.set_proctab()
        if ret == False:
            return False
        executable = self.proctab.executable_path
        self.executable_path = executable
        self.cancel = False
        while os.path.exists(self.executable_path) == False and self.cancel == False:
            # Open dialog prompting for base path
            dialog = gtk.Dialog('Please enter the full path for the executable')
            label = gtk.Label('Failed to find executable:\n%s\nPlease enter the executable full path' %self.executable_path)
            dialog.vbox.pack_start(label, True, True, 0)
            entry = gtk.Entry()
            entry.set_max_length(8192)
            def activate_entry(entry, dialog):
                self.executable_path = entry.get_text()
                dialog.destroy()
            entry.connect("activate", lambda w: activate_entry(entry, dialog))
            dialog.vbox.pack_start(entry, True, True, 0)
            hbox = gtk.HButtonBox()
            button = gtk.Button(stock = gtk.STOCK_CANCEL)
            def cancel_entry(dialog):
                entry.set_text('cancel_operation')
                self.cancel = True
                dialog.destroy()
            button.connect("clicked", lambda w: cancel_entry(dialog))
            hbox.pack_start(button, False, False, 0)
            button = gtk.Button(stock = gtk.STOCK_OK)
            button.connect("clicked", lambda w: activate_entry(entry, dialog))
            hbox.pack_start(button, False, False, 0)
            dialog.vbox.pack_start(hbox, False, False, 0)
            dialog.show_all()
            dialog.run()
            if entry.get_text() == 'cancel_operation':
                dialog.destroy()
                return False
        if self.cancel == True:
            return False
        pids = []
        hosts = []
        exes = []
        for rank, host, pid, exe_index in self.proctab.process_list:
            if rank in subset_list:
                pids.append(pid)
                hosts.append(host)
                exes.append(self.proctab.executable_paths[exe_index])
        arg_list = []
        if debugger == 'TotalView':
            filepath = self.options['TotalView Path']
            if not filepath or not os.access(filepath, os.X_OK):
                show_error_dialog('Failed to locate executable totalview\ndefault: %s\n' %filepath, self)
                return
            arg_list.append(filepath)
            cli_attach = False
            if self.STAT != None:
                if self.STAT.getApplicationOption() == STAT_SERIAL_ATTACH:
                    cli_attach = True
            if cli_attach == True:
                arg_list.append('-e')
                for counter, rank in enumerate(subset_list):
                    if counter == 0:
                        cli_command = "set new_pid [dattach -r %s -e %s %d] ;" %(hosts[counter], exes[counter], pids[counter])
                    else:
                        cli_command += " dattach -r %s -g $CGROUP($new_pid) -e %s %d ;" %(hosts[counter], exes[counter], pids[counter])
                arg_list.append(cli_command)
            else:
                arg_list.append('-parallel_attach')
                arg_list.append('yes')
                if self.options['Remote Host'] not in ['localhost', '']:
                    arg_list.append('-remote')
                    arg_list.append(self.options['Remote Host'])
                arg_list.append('-pid')
                arg_list.append(str(self.proctab.launcher_pid))
                str_list = ''
                for rank in subset_list:
                    str_list += '%s ' %(rank)
                arg_list.append('-default_parallel_attach_subset=%s' %(str_list))
                arg_list.append(self.options['Launcher Exe'])
        elif debugger == 'DDT':
            filepath = self.options['DDT Path']
            if not filepath or not os.access(filepath, os.X_OK):
                show_error_dialog('Failed to locate executable ddt\ndefault: %s\n' %filepath, self)
                return

            # Look for LaunchMON installation for DDT 2.5+
            ddt_lmon_prefix = self.options['DDT LaunchMON Prefix']
            ddt_lmon_lib = '%s/lib' %(ddt_lmon_prefix)
            ddt_lmon_launchmon = '%s/bin/launchmon' %(ddt_lmon_prefix)
            if not ddt_lmon_launchmon or not os.access(ddt_lmon_launchmon, os.X_OK) or not ddt_lmon_lib or self.serial_attach == True:
                # DDT 2.4 / DDT 2.5+ w/o LanchMON / serial processes
                arg_list.append(filepath)
                arg_list.append('-attach')
                arg_list.append(self.executable_path)
                for counter, pid in enumerate(pids):
                    arg_list.append('%s:%d' %(hosts[counter], pid))
            else:
                # DDT 2.5+ with LaunchMON
                arg_list.append("env")
                arg_list.append("LD_LIBRARY_PATH=%s" %(ddt_lmon_lib))
                arg_list.append("LMON_LAUNCHMON_ENGINE_PATH=%s" %(ddt_lmon_launchmon))
                arg_list.append(filepath)
                arg_list.append("-attach-mpi")
                arg_list.append(str(self.proctab.launcher_pid))
                arg_list.append("-subset")
                rank_list_arg = ''
                for rank in subset_list:
                    if rank == subset_list[0]:
                        rank_list_arg += '%d' %rank
                    else:
                        rank_list_arg += ',%d' %rank
                arg_list.append(rank_list_arg)
                arg_list.append(self.executable_path)

        for arg in self.options['Additional Debugger Args'].split():
            arg_list.insert(-1, arg)

        # First Detach STAT!!!
        if self.STAT != None:
            stop_list = intArray(len(subset_list))
            i = -1
            for rank in subset_list:
                i += 1
                stop_list[i] = rank
            self.on_detach(None, stop_list, len(subset_list))
        
        sys.stdout.write('fork exec %s %s\n' %(debugger, arg_list))
        if os.fork() == 0:
            self.exec_and_exit(arg_list)

    def get_full_edge_label(self, widget, button_clicked, node):
        edge_label = self.STAT.getNodeInEdge(int(node.node_name))
        old_label = node.edge_label
        node.edge_label = edge_label
        task_list = get_task_list(edge_label)
        node.num_leaf_tasks = -1
        if node.edge_label_id in STATview.task_label_id_to_list:
            STATview.task_label_id_to_list[node.edge_label_id] = task_list
        if old_label in STATview.task_label_to_list:
            del STATview.task_label_to_list[old_label]
        for inode in self.get_current_graph().nodes:
            if inode.edge_label == old_label:
                inode.edge_label = edge_label
                inode.num_leaf_tasks = -1
                if inode.edge_label_id in STATview.task_label_id_to_list:
                    STATview.task_label_id_to_list[inode.edge_label_id] = task_list
        try:
            self.my_dialog.destroy()
        except:
            pass
        self.on_node_clicked(widget, button_clicked, node)

    def on_fatal_error(self):
        """Handle fatal error."""
        self.set_action_sensitivity('new')
        self.STAT.shutDown()
        self.STAT = None
        self.reattach = False
        self.attached = False
        self.show_all()

    def exec_and_exit(self, arg_list):
        """Run a command and exits."""
        subprocess.call(arg_list)
        sys.exit(0)

    def pack_sample_options(self, vbox, multiple, attach=False):
        """Pack the sample options into the specified vbox."""
        frame = gtk.Frame('Per Sample Options')
        vbox2 = gtk.VBox()
        self.pack_check_button(vbox2, 'With Threads', False, False, 5)
        self.pack_check_button(vbox2, 'Gather Python Traces', False, False, 5)
        frame2 = gtk.Frame('Stack Frame (node) Sample Options')
        vbox3 = gtk.VBox()
        self.pack_radio_buttons(vbox3, 'Sample Type')
        frame2.add(vbox3)
        vbox2.pack_start(frame2, False, False, 5)
        frame2 = gtk.Frame('Process Set (edge) Sample Options')
        vbox3 = gtk.VBox()
        self.pack_radio_buttons(vbox3, 'Edge Type')
        frame2.add(vbox3)
        vbox2.pack_start(frame2, False, False, 5)

        if attach == False:
            self.pack_spinbutton(vbox2, 'Run Time Before Sample (sec)')
        expander = gtk.Expander("Advanced")
        hbox = gtk.HBox()
        self.pack_spinbutton(hbox, 'Num Retries')
        self.pack_spinbutton(hbox, 'Retry Frequency (us)')
        expander.add(hbox)
        vbox2.pack_start(expander, False, False, 5)
        frame.add(vbox2)
        vbox.pack_start(frame, False, False, 5)
        if multiple:
            frame = gtk.Frame('Multiple Sample Options')
            vbox2 = gtk.VBox()
            hbox = gtk.HBox()
            self.pack_spinbutton(hbox, 'Num Traces')
            self.pack_spinbutton(hbox, 'Trace Frequency (ms)')
            vbox2.pack_start(hbox, False, False, 5)
            self.pack_check_button(vbox2, 'Gather Individual Samples')
            self.pack_check_button(vbox2, 'Clear On Sample')
            frame.add(vbox2)
            vbox.pack_start(frame, False, False, 5)

def STATGUI_main(argv):
    """The STATGUI main."""
    window = STATGUI()
    STATview.window = window
    window.connect('destroy', window.on_destroy)
    #gtk.gdk.threads_init()
    gtk.main()

if __name__ == '__main__':
    STATGUI_main(sys.argv)
