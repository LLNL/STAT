#!/bin/env python

"""@package STATGUI
A GUI for driving the Stack Trace Analysis Tool."""

__copyright__ = """Copyright (c) 2007-2008, Lawrence Livermore National Security, LLC."""
__license__ = """Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee <lee218@llnl.gov>, Dorian Arnold, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-400455.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
        
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
__author__ = ["Gregory Lee <lee218@llnl.gov>", "Dorian Arnold", "Dong Ahn", "Bronis de Supinski", "Barton Miller", "Martin Schulz"]
__version__ = "1.0.0"

import STATview
from STATview import *
import sys, DLFCN
sys.setdlopenflags(DLFCN.RTLD_NOW | DLFCN.RTLD_GLOBAL)
from STAT import *
import commands
import shelve
import time
import string


## \param var_spec - the variable specificaiton (location and name)
#  \return a string representation of the variable specificaiton
#
#  \n
def var_spec_to_string(var_spec):
    """Translates a variable specificaiton list into a string."""
    if var_spec == []:  
        return 'NULL'
    ret = '%d#' %len(var_spec)
    for file, line, depth, var in var_spec:
        ret += '%s:%d.%d$%s,' %(file, line, depth, var)
    ret = ret[:len(ret) - 1]
    return ret


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
        self.proctabfile = None
        self.attached = False
        self.reattach = False
        self.combo_boxes = {}
        self.spinners = {}
        self.types = {}
        self.sample_task_list = ['Sample Stack Traces', 'Gather Stack Traces', 'Render Stack Traces']
        self.attach_task_list = ['Launch Daemons', 'Connect to Daemons', 'Attach to Application']
        self.attach_task_list += self.sample_task_list
        self.types['Topology Type'] = ['automatic', 'depth', 'max fanout', 'custom']
        self.types['Verbosity Type'] = ['error', 'stdout', 'full']
        self.types['Sample Type'] = ['function only', 'function and pc', 'function and line']
        self.types['Remote Host Shell'] = ['rsh', 'ssh']
        self.options = {}
        self.options['Remote Host'] = "localhost"
        self.options['Remote Host Shell'] = "rsh"
        self.options['PID'] = None
        self.options['Launcher Exe'] = ''
        self.options['Topology Type'] = 'automatic'
        self.options['Topology'] = '1'
        self.options['Share App Nodes'] = False
        self.options['Tool Daemon Path'] = self.STAT.getToolDaemonExe()
        self.options['Filter Path'] = self.STAT.getFilterPath()
        self.options['Job Launcher'] = 'mpirun'
        self.options['Log Dir'] = os.environ['HOME']
        self.options['Log Frontend'] = False
        self.options['Log Backend'] = False
        if 'STAT_LMON_DEBUG_BES' in os.environ:
            self.options['Debug Backends'] = True
        else:
            self.options['Debug Backends'] = False
        self.options['Verbosity Type'] = 'error'
        self.options['Communication Nodes'] = ''
        self.options['Communication Processes per Node'] = 8
        self.options['Num Traces'] = 10
        self.options['Trace Frequency (ms)'] = 1000
        self.options['Num Retries'] = 5
        self.options['Retry Frequency (ms)'] = 10
        self.options['With Threads'] = False
        self.options['Clear On Sample'] = True
        self.options['Gather Individual Samples'] = False
        self.options['Run Time Before Sample (sec)'] = 0
        self.options['Sample Type'] = 'function only'
        self.options['DDT Path'] = STATview._which('ddt')
        self.options['DDT LaunchMON Prefix'] = '/usr/local'
        self.options['TotalView Path'] = STATview._which('totalview')
        # Check for site default options then for user default options
        site_options_path = '%s/etc/STAT/STAT.conf' %self.STAT.getInstallPrefix()
        user_options_path = '%s/.STATrc' %(os.environ.get('HOME'))
        for path in [site_options_path, user_options_path]:
            if os.path.exists(path):
                try:
                    f = open(path, 'r')
                except:
                    sys.stderr.write('failed to open preferences file %s\n' %(path))
                    continue
                for line in f.readlines():
                    if line[0] == '#':
                        continue
                    split_line = line.split('=')
                    if len(split_line) != 2:
                        sys.stderr.write('invalid preference specification %s in file %s\n' %(line.strip('\n'), path))
                        continue
                    option = string.lstrip(string.rstrip(split_line[0]))
                    value = string.lstrip(string.rstrip(split_line[1]))
                    if option in self.options.keys():
                        if type(self.options[option]) == type(1):
                            value = int(value)
                        elif type(self.options[option]) == type(True):
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
                            STATview.search_paths['source'].append(value)
                        else:
                            sys.stderr.write('search path %s specified in %s is not accessible\n' %(value, path))
                    elif option == 'Include Search Path':
                        if os.path.exists(value):
                            STATview.search_paths['include'].append(value)
                        else:
                            sys.stderr.write('search path %s specified in %s is not accessible\n' %(value, path))
                    else:
                        sys.stderr.write('invalid option %s in file %s\n' %(option, path))

        self.var_spec = []
        self.filter_entry = None
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
        lines.insert(count, '            <menuitem action="SavePrefs"/>') 
        lines.insert(count, '            <menuitem action="LoadPrefs"/>') 
        lines.insert(count, '            <separator/>') 
        lines.insert(count, '            <menuitem action="Properties"/>') 
        lines.insert(count, '            <separator/>') 
        STATDotWindow.ui = ''
        for line in lines:
            STATDotWindow.ui += line + '\n'
        menu_actions = []
        menu_actions.append(('LoadPrefs', gtk.STOCK_OPEN, '_Load Preferences', '<control>L', 'Save current preference settings', self.on_load_prefs))
        menu_actions.append(('SavePrefs', gtk.STOCK_SAVE_AS, 'Save _Preferences', '<control>P', 'Load saved preference settings', self.on_save_prefs))
        menu_actions.append(('Properties', gtk.STOCK_PROPERTIES, 'P_roperties', '<control>R', 'View application properties', self.on_properties))
        menu_actions.append(('About', gtk.STOCK_ABOUT, None, None, None, self.on_about))
        STATDotWindow.__init__(self, menu_actions)
        self.set_title('STAT')
        self.on_attach(None)

    def on_about(self, action):
        """Display info about STAT."""
        about_dialog = gtk.AboutDialog()
        about_dialog.set_name('STAT')
        about_dialog.set_authors(__author__)
        about_dialog.set_copyright(__copyright__)
        about_dialog.set_license(__license__)
        about_dialog.set_wrap_license(80)
        version = intArray(3)
        if self.STAT == None:
            self.STAT = STAT_FrontEnd()
        self.STAT.getVersion(version)
        about_dialog.set_version('%d.%d.%d' %(version[0], version[1], version[2]))
        try:
            pixbuf = gtk.gdk.pixbuf_new_from_file(STAT_LOGO)
            about_dialog.set_logo(pixbuf)
        except gobject.GError, error:
            pass
        about_dialog.set_website('https://outreach.scidac.gov/projects/stat/')
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

    def on_load_prefs(self, action):
        """Load user-saved preferences from a file."""
        chooser = gtk.FileChooserDialog(title = "Load Preferences", action = gtk.FILE_CHOOSER_ACTION_OPEN, buttons = (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_SAVE_AS, gtk.RESPONSE_OK))
        chooser.set_default_response(gtk.RESPONSE_OK)
        filter = gtk.FileFilter()
        filter.set_name('STAT Prefs File')
        filter.add_pattern("*.SPF")
        chooser.add_filter(filter)
        filter = gtk.FileFilter()
        filter.set_name('All files')
        filter.add_pattern("*")
        chooser.add_filter(filter)
        chooser.set_current_folder('%s/.STAT' %os.environ['HOME'])
        if chooser.run() == gtk.RESPONSE_OK:
            filename = chooser.get_filename() 
            try:
                shelf = shelve.open(filename)
                for key in self.options.keys():
                    self.options[key] = shelf[key]
                shelf.close()    
            except:
                show_error_dialog('Failed to load preferences file %s\n' %filename, self)
        chooser.destroy()    

    def on_save_prefs(self, action):        
        """Save user preferences to a file."""
        chooser = gtk.FileChooserDialog(title = "Save Preferences", action = gtk.FILE_CHOOSER_ACTION_SAVE, buttons = (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_SAVE_AS, gtk.RESPONSE_OK))
        chooser.set_default_response(gtk.RESPONSE_OK)
        chooser.set_do_overwrite_confirmation(True)
        filter = gtk.FileFilter()
        filter.set_name('STAT Prefs File')
        filter.add_pattern("*.SPF")
        chooser.add_filter(filter)
        filter = gtk.FileFilter()
        filter.set_name('All files')
        filter.add_pattern("*")
        chooser.add_filter(filter)
        chooser.set_current_folder('%s/.STAT' %os.environ['HOME'])
        if chooser.run() == gtk.RESPONSE_OK:
            filter = chooser.get_filter()
            ext = ''
            if filter.get_name() == 'STAT Prefs File':
                ext = '.SPF'
            filename = chooser.get_filename() + ext
            try:
                shelf = shelve.open(filename)
                for key in self.options.keys():
                    shelf[key] = self.options[key]
                shelf.close()    
            except:
                show_error_dialog('Failed to save preferences file %s\n' %filename, self)
        chooser.destroy()    

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
        out_dir = self.STAT.getOutDir()
        file_prefix = self.STAT.getFilePrefix()
        ptab_file = out_dir + '/' + file_prefix + '.ptab'
        try:
            f = open(ptab_file, 'r')
            lines = f.readlines()
            # resort by MPI rank (instead of hostname)
            process_table = lines[0] 
            entries = range(len(lines[1:]))
            for line in lines[1:]:
                num = int(line.split()[0])
                entries[num] = line
            for entry in entries:
                process_table += entry
        except:
            process_table = ''
        self.properties_window = gtk.Window()
        self.properties_window.set_title('Properties')
        self.properties_window.connect('destroy', self.on_properties_destroy)
        vbox = gtk.VBox()

        frame = gtk.Frame('Application Executable')
        text_view = gtk.TextView()
        text_buffer = gtk.TextBuffer()
        text_buffer.set_text(appl_exe)
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

        frame = gtk.Frame('Process Table')
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

        self.properties_window.add(vbox)
        self.properties_window.show_all()
       
    def on_properties_destroy(self, action):
        """Clean up the properties window."""
        self.properties_window = None

    def pid_toggle_cb(self, action, pid, command):
        """A callback to modify the PID option."""
        self.options['PID'] = pid
        self.options['Launcher Exe'] = command

    def on_update_process_listing(self, widget, frame, attach_dialog, filter=None):
        """Generate a wait dialog and search for user processes."""
        stat_wait_dialog.show_wait_dialog_and_run(self._on_update_process_listing, (widget, frame, attach_dialog, filter), [], attach_dialog)

    def _on_update_process_listing(self, widget, frame, attach_dialog, filter=None):
        """Searche for user processes."""
        self.options['Remote Host Shell'] = self.types['Remote Host Shell'][self.combo_boxes['Remote Host Shell'].get_active()]
        vbox = gtk.VBox()
        if self.options['Remote Host'] == 'localhost' or self.options['Remote Host'] == '':
            output = commands.getoutput('ps xw')
        else:    
            output = commands.getoutput('%s %s ps xw' %(self.options['Remote Host Shell'], self.options['Remote Host']))
            if output.find('Hostname not found') != -1 or output.find('PID') == -1:
                show_error_dialog('Failed to get process listing for %s' %self.options['Remote Host'], attach_dialog)
                return False
        output = output.split('\n')
        line = output[0].split()
        pid_index = 0
        command_index = 0
        counter = 0
        for token in line:
            if token == 'PID':
                pid_index = counter
            elif token == 'COMMAND':
                command_index = counter
            counter += 1
        counter = 0
        for line in output[1:]:
            text = '% 5d ' %int(line.split()[pid_index])
            for token in line.split()[command_index:]:
                text += ' %s' %token
            if filter != None:
                if filter.get_text() != '':
                    if text.find(filter.get_text()) == -1:
                        continue
            if counter == 0:
                radio_button = gtk.RadioButton(None, text)
                radio_button.set_active(True)
                self.options['PID'] = int(line.split()[pid_index])
                self.options['Launcher Exe'] = line.split()[command_index]
            else:    
                radio_button = gtk.RadioButton(radio_button, text)
            radio_button.connect("toggled", self.pid_toggle_cb, int(line.split()[pid_index]), line.split()[command_index])
            vbox.pack_start(radio_button, False, False, 0)
            counter += 1
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

    def on_update_remote_host(self, w, frame, attach_dialog, entry):
        """Callback to search a remote host for processes."""
        self.options['Remote Host'] = entry.get_text()
        entry.set_text(self.options['Remote Host'])
        self.on_update_process_listing(w, frame, attach_dialog, self.filter_entry)

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
        self.attach_cb(None, None)
        return True

    def on_attach(self, action):
        """Generate a dialog to attach to a new job."""
        if self.reattach == True and self.STAT != None:
            self.STAT.shutDown()
            self.STAT = None
        self.STAT = STAT_FrontEnd()
        self.options['PID'] = None
        self.options['Launcher Exe'] = ''
        attach_dialog = gtk.Dialog('Attach', self)
        attach_dialog.set_default_size(400, 400)
        notebook = gtk.Notebook()
        notebook.set_tab_pos(gtk.POS_TOP)
        notebook.set_scrollable(False)
        process_frame = gtk.Frame('Current Processes')
        vbox = gtk.VBox()
        self.pack_combo_box(vbox, 'Remote Host Shell')
        self.pack_entry_and_button(self.options['Remote Host'], self.on_update_remote_host, process_frame, attach_dialog, "Search Remote Host", vbox)
        vbox.pack_start(process_frame, True, True, 0)
        hbox = gtk.HBox()
        button = gtk.Button(stock = gtk.STOCK_REFRESH)
        button.connect("clicked", lambda w: self.on_update_process_listing(w, process_frame, attach_dialog, self.filter_entry))
        hbox.pack_start(button, False, False, 10)
        self.filter_entry = self.pack_entry_and_button(self.options['Job Launcher'], self.on_update_process_listing, process_frame, attach_dialog, "Filter", hbox, True, True, 0)
        vbox.pack_start(hbox, False, False, 0)
        vbox.pack_start(gtk.HSeparator(), False, False, 0)
        hbox = gtk.HButtonBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_end(button, False, False, 0)
        button = gtk.Button("Attach")
        button.connect("clicked", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.attach_cb, (w, attach_dialog), self.attach_task_list, attach_dialog))
        hbox.pack_end(button, False, False, 0)
        vbox.pack_start(hbox, False, False, 0)
        label = gtk.Label('Attach')
        notebook.append_page(vbox, label)

        vbox = gtk.VBox()
        hbox=gtk.HBox()
        hbox.pack_start(gtk.Label('Enter application launch string:'), False, False, 0)
        vbox.pack_start(hbox, False, False, 10)
        entry = gtk.Entry()
        entry.set_max_length(8192)
        entry.connect("activate", lambda w: self.launch_application_cb(entry, w, attach_dialog))
        vbox.pack_start(entry, False, False, 0)
        hbox = gtk.HButtonBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_end(button, False, False, 0)
        button = gtk.Button("Launch")
        button.connect("clicked", lambda w: stat_wait_dialog.show_wait_dialog_and_run(self.launch_application_cb, (entry, w, attach_dialog), self.attach_task_list, attach_dialog))
        hbox.pack_start(button, False, False, 0)
        vbox.pack_end(hbox, False, False, 0)
        label = gtk.Label('Launch')
        notebook.append_page(vbox, label)

        vbox = gtk.VBox()
        self.pack_sample_options(vbox, False, True)
        hbox = gtk.HBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: self.on_cancel_attach(w, attach_dialog))
        hbox.pack_start(button, False, False, 0)
        vbox.pack_end(hbox, False, False, 0)
        label = gtk.Label('Sample Options')
        notebook.append_page(vbox, label)

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
        self.pack_string_option(vbox2, 'Log Dir', attach_dialog)
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
        attach_dialog.vbox.pack_start(notebook, True, True, 0)
        attach_dialog.show_all()
        attach_dialog.run()
        self.sw = None

    def launch_application_cb(self, entry, widget, attach_dialog):
        """Callback to launch an application under STAT's control."""
        args = entry.get_text()
        args = args.split()
        self.options['Launcher Exe'] = args[0]
        for arg in args:
            self.STAT.addLauncherArgv(arg)
        self.attach_cb(widget, attach_dialog, True)

    def attach_cb(self, widget, attach_dialog, launch=False):
        """Callback to attach to a job and gather a stack trace."""
        stat_wait_dialog.update_progress_bar(0.01)
        if attach_dialog != None:
            attach_dialog.destroy()
        self.options['Topology Type'] = self.types['Topology Type'][self.combo_boxes['Topology Type'].get_active()]
        self.options['Verbosity Type'] = self.types['Verbosity Type'][self.combo_boxes['Verbosity Type'].get_active()]
        self.options['Communication Processes per Node'] = int(self.spinners['Communication Processes per Node'].get_value())
        if self.STAT == None:
            self.STAT = STAT_FrontEnd()
        if launch == False:
            if self.options['PID'] == None:
                show_error_dialog('No job selected.  Please select a valid\njob launcher process to attach to.\n', self)
                return False


        self.STAT.setToolDaemonExe(self.options['Tool Daemon Path'])
        self.STAT.setFilterPath(self.options['Filter Path'])
        logType = STAT_LOG_NONE
        if self.options['Log Frontend'] and self.options['Log Backend']:
            logType = STAT_LOG_ALL
        elif self.options['Log Frontend']:
            logType = STAT_LOG_FE
        elif self.options['Log Backend']:
            logType = STAT_LOG_BE
        if self.options['Log Frontend'] or self.options['Log Backend']:
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

        if launch == False:
            if self.options['Remote Host'] != "localhost":
                ret = self.STAT.attachAndSpawnDaemons(self.options['PID'], self.options['Remote Host'])
            else:
                ret = self.STAT.attachAndSpawnDaemons(self.options['PID'])
        else:
            if self.options['Remote Host'] != "localhost":
                ret = self.STAT.launchAndSpawnDaemons(self.options['Remote Host'])
            else:
                ret = self.STAT.launchAndSpawnDaemons()
        if 'LMON_DEBUG_BES' in os.environ:
            del os.environ['LMON_DEBUG_BES']
        if ret != STAT_OK:
            show_error_dialog('Failed to Launch Daemons:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return False
        stat_wait_dialog.update(0.10)

        topology_type = STAT_TOPOLOGY_DEPTH
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
        if self.attached == False:
            self.STAT = None
            self.reattach = False
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
                show_error_dialog('Failed to sample stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
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
                show_error_dialog('Failed to sample stack trace:\n%s' %self.STAT.getLastErrorMessage(), self)
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
        self.options['Retry Frequency (ms)'] = int(self.spinners['Retry Frequency (ms)'].get_value())
        try:
            self.options['Run Time Before Sample (sec)'] = int(self.spinners['Run Time Before Sample (sec)'].get_value())
        except:
            pass

    def sleep(self, seconds):
        """Sleep for specified time with a progress bar."""
        for i in range(int(seconds * 100)):
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
        self.update_sample_options()
        ret = self.STAT.pause()
        stat_wait_dialog.update_progress_bar(0.35)
        self.set_action_sensitivity('paused')
        if ret != STAT_OK:
            show_error_dialog('Failed to pause application during sample stack trace operation:\n%s' %self.STAT.getLastErrorMessage(), self)
            self.on_fatal_error()
            return ret
        sample_type = STAT_FUNCTION_NAME_ONLY
        if self.options['Sample Type'] == 'function and pc':
            sample_type = STAT_FUNCTION_AND_PC
        elif self.options['Sample Type'] == 'function and line':
            sample_type = STAT_FUNCTION_AND_LINE
        ret = self.STAT.sampleStackTraces(sample_type, self.options['With Threads'], self.options['Clear On Sample'], 1, 1, self.options['Num Retries'], self.options['Retry Frequency (ms)'], False, var_spec_to_string(self.var_spec))
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
            f = file(filename, 'rt')
        except:
            show_error_dialog('Failed to open file %s' %filename, self)
            return ret
        self.tabs[self.notebook.get_current_page()].history_view.get_buffer().set_text('')
        self.set_dotcode(f.read(), filename, self.notebook.get_current_page())
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

        for i in range(self.options['Num Traces']):
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

            sample_type = STAT_FUNCTION_NAME_ONLY
            if self.options['Sample Type'] == 'function and pc':
                sample_type = STAT_FUNCTION_AND_PC
            elif self.options['Sample Type'] == 'function and line':
                sample_type = STAT_FUNCTION_AND_LINE
            ret = self.STAT.sampleStackTraces(sample_type, self.options['With Threads'], self.options['Clear On Sample'], 1, 0, self.options['Num Retries'], self.options['Retry Frequency (ms)'], False, var_spec_to_string(self.var_spec))
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
                    f = file(filename, 'rt')
                except:
                    show_error_dialog('Failed to open file %s' %filename, self)
                    return False
                page = self.notebook.get_current_page()
                self.create_new_tab(page + 1)
                self.notebook.set_current_page(page + 1)
                self.tabs[self.notebook.get_current_page()].history_view.get_buffer().set_text('')
                self.set_dotcode(f.read(), filename, self.notebook.get_current_page())

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
                for i in range(int(self.options['Trace Frequency (ms)'] / 10)):
                    time.sleep(.01)
                    if gtk.events_pending():
                        gtk.main_iteration()
        stat_wait_dialog.update(0.91)
        ret = self.STAT.gatherTraces(False)
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
        stat_wait_dialog.update(0.95)
        filename = self.STAT.getLastDotFilename()
        try:
            f = file(filename, 'rt')
        except:
            show_error_dialog('Failed to open file %s' %filename, self)
            return False
        if self.options['Gather Individual Samples'] == True:
            page = self.notebook.get_current_page()
            self.create_new_tab(page + 1)
            self.notebook.set_current_page(page + 1)
        else:
            self.tabs[self.notebook.get_current_page()].history_view.get_buffer().set_text('')
        self.set_dotcode(f.read(), filename, self.notebook.get_current_page())
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
            if task_list == []:
                continue
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
            rep_button = gtk.RadioButton(None)
            all_button = gtk.RadioButton(rep_button)
            none_button = gtk.RadioButton(rep_button)
            rep_button.connect('toggled', lambda w: self.on_toggle_eq(w, classes.index(item), 'rep_button'))
            all_button.connect('toggled', lambda w: self.on_toggle_eq(w, classes.index(item), 'all_button'))
            none_button.connect('toggled', lambda w: self.on_toggle_eq(w, classes.index(item), 'none_button'))
            rep_button.set_active(True)
            hbox = gtk.HBox()
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

    def launch_debugger_cb(self, widget, args):
        """Callback to launch full-featured debugger on a subset of tasks."""
        debugger, eq_dialog = args
        eq_dialog.destroy()
        subset_list = []
        for item in self.eq_state['classes']:
            if item['rep_button'].get_active() == True:
                subset_list.append(item['class'][0])
            elif item['all_button'].get_active() == True:
                subset_list += item['class']
        additional_tasks = '[' + self.eq_state['entry'].get_text().strip(' ').strip('[').strip(']') + ']'
        task_list_set = set(get_task_list(additional_tasks) + subset_list)
        subset_list = list(task_list_set)
        # use current STAT session to determine job launcher PID
        if self.proctabfile == None:
            try:
                out_dir = self.STAT.getOutDir()
                file_prefix = self.STAT.getFilePrefix()
                self.proctabfile = out_dir + '/' + file_prefix + '.ptab'
            except:
                self.proctabfile = ''
                pass
            if not os.path.exists(self.proctabfile):
                directory = os.path.dirname(os.path.abspath(self.get_current_graph().cur_filename))
                self.proctabfile = ''
                for file in os.listdir(directory):
                    if file.find('.ptab') != -1:
                        self.proctabfile = directory + '/' + file
                        break
            if self.proctabfile == '':
                show_error_dialog('Failed to find process table.ptab file for this call tree.', self)
                return False
        try:
            f = open(self.proctabfile, 'r')
        except:
            show_error_dialog('failed to open process table file:\n\n%s\n\nPlease be sure that it is a valid process table file outputted from STAT.' %self.proctabfile, self)
            return False
        lines = f.readlines()
        launcher = lines[0].strip('\n')
        lines = lines[1:]
        pids = []
        executable = lines[0].split()[2]
        executable_path = executable
        cancel = False
        while os.path.exists(executable_path) == False:
            # Open dialog prompting for base path
            dialog = gtk.Dialog('Please enter the base path for the executable')
            label = gtk.Label('Failed to find executable:\n%s\nPlease enter the base path' %executable)
            dialog.vbox.pack_start(label, True, True, 0)
            entry = gtk.Entry()
            entry.set_max_length(8192)
            entry.connect("activate", lambda w: dialog.destroy())
            dialog.vbox.pack_start(entry, True, True, 0)
            hbox = gtk.HButtonBox()
            button = gtk.Button(stock = gtk.STOCK_CANCEL)
            def cancel_entry(dialog):
                entry.set_text('cancel_operation')
                dialog.destroy()
            button.connect("clicked", lambda w: cancel_entry(dialog))
            hbox.pack_start(button, False, False, 0)
            button = gtk.Button(stock = gtk.STOCK_OK)
            button.connect("clicked", lambda w: dialog.destroy())
            hbox.pack_start(button, False, False, 0)
            dialog.vbox.pack_start(hbox, False, False, 0)
            dialog.show_all()
            dialog.run()
            if entry.get_text() == 'cancel_operation':
                dialog.destroy()
                return False
            executable_path = entry.get_text() + '/' + executable
        for line in lines:
            line = line.split()
            for rank in subset_list:
                if int(line[0]) == rank:
                    pids.append(line[1])
                    break
        arg_list = []
        if debugger == 'TotalView':
            filepath = self.options['TotalView Path']
            if not filepath or not os.access(filepath, os.X_OK):
                show_error_dialog('Failed to locate executable totalview\ndefault: %s\n' %filepath, self)
                return
            arg_list.append(filepath)
            arg_list.append('-parallel_attach')
            arg_list.append('yes')
            if self.options['Remote Host'] not in ['localhost', '']:
                arg_list.append('-remote')
                arg_list.append(self.options['Remote Host'])
            arg_list.append('-pid')
            arg_list.append(launcher[launcher.find(':')+1:])
            str_list = ''
            for rank in subset_list:
                str_list += '%s ' %(rank)
            arg_list.append('-default_parallel_attach_subset=%s' %(str_list))
            # "-e g" added for BlueGene work around
            arg_list.append('-e')
            arg_list.append('g')
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
            if not ddt_lmon_launchmon or not os.access(ddt_lmon_launchmon, os.X_OK) or not ddt_lmon_lib:
                # DDT 2.4 / DDT 2.5+ w/o LanchMON
                print ddt_lmon_launchmon, ddt_lmon_lib, os.access(ddt_lmon_launchmon, os.X_OK)
                arg_list.append(filepath)
                arg_list.append('-attach')
                arg_list.append(executable_path)
                for pid in pids:
                    arg_list.append(pid)
            else:
                # DDT 2.5+ with LaunchMON
                arg_list.append("env")
                arg_list.append("LD_LIBRARY_PATH=%s" %(ddt_lmon_lib))
                arg_list.append("LMON_LAUNCHMON_ENGINE_PATH=%s" %(ddt_lmon_launchmon))
                arg_list.append(filepath)
                arg_list.append("-attach-mpi")
                arg_list.append(launcher[launcher.find(':')+1:])
                arg_list.append("-subset")
                rank_list_arg = ''
                for rank in subset_list:
                    if rank == subset_list[0]:
                        rank_list_arg += '%d' %rank
                    else:
                        rank_list_arg += ',%d' %rank
                arg_list.append(rank_list_arg)
                arg_list.append(executable_path)

        sys.stdout.write('fork exec %s %s\n' %(debugger, arg_list))
        # First Detach STAT!!!
        if self.STAT != None:
            stop_list = intArray(len(subset_list))
            i = -1
            for rank in subset_list:
                i += 1
                stop_list[i] = rank
            self.on_detach(None, stop_list, len(subset_list))
        if os.fork() == 0:
            self.exec_and_exit(arg_list)

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

    def update_option(self, w, label, parent_window, option):        
        """Generate text entry dialog to update the specified option."""
        dialog = gtk.Dialog('Update %s' %option, parent_window)
        entry = gtk.Entry()
        entry.set_max_length(1024)
        entry.set_text(self.options[option])
        entry.connect("activate", lambda w: self.on_update_option(w, entry, label, dialog, option))
        dialog.vbox.pack_start(entry, True, True, 0)
        hbox = gtk.HButtonBox()
        button = gtk.Button(stock = gtk.STOCK_CANCEL)
        button.connect("clicked", lambda w: dialog.destroy())
        hbox.pack_start(button, False, False, 0)
        button = gtk.Button(stock = gtk.STOCK_OK)
        button.connect("clicked", lambda w: self.on_update_option(w, entry, label, dialog, option))
        hbox.pack_start(button, False, False, 0)
        dialog.vbox.pack_start(hbox, False, False, 0)
        dialog.show_all()
        dialog.run()

    def on_update_option(self, w, entry, label, dialog, option):
        """Callback to update the specified option."""
        self.options[option] = entry.get_text()
        entry.set_text('')
        label.set_text('%s: %s' %(option, self.options[option]))
        dialog.destroy()

    def pack_entry_and_button(self, entry_text, function, frame, dialog, button_text, box, fill=False, center=False, pad=0):
        """Generates a text entry and activation button."""
        hbox = gtk.HBox()
        entry = gtk.Entry()
        entry.set_max_length(1024)
        entry.set_text(entry_text)
        entry.connect("activate", lambda w: apply(function, (w, frame, dialog, entry)))
        hbox.pack_start(entry, True, True, 0)
        button = gtk.Button(button_text)
        button.connect("clicked", lambda w: apply(function, (w, frame, dialog, entry)))
        hbox.pack_start(button, False, False, 0)
        box.pack_start(hbox, fill, center, pad)
        return entry

    def pack_radio_buttons(self, box, option):
        """Pack a set of radio buttons for a specified option."""
        for type in self.types[option]:
            if type == self.types[option][0]:
                radio_button = gtk.RadioButton(None, type)
            else:
                radio_button = gtk.RadioButton(radio_button, type)
            if type == self.options[option]:
                radio_button.set_active(True)
                self.toggle_radio_button(None, (option, type))
            radio_button.connect('toggled', self.toggle_radio_button, (option, type))
            box.pack_start(radio_button, False, False, 0)

    def toggle_radio_button(self, action, data):
        """Callback to toggle on/off a radio button."""
        option, type = data
        self.options[option] = type

    def pack_sample_options(self, vbox, multiple, attach=False):
        """Pack the sample options into the specified vbox."""
        frame = gtk.Frame('Per Sample Options')
        vbox2 = gtk.VBox()
        self.pack_check_button(vbox2, 'With Threads', False, False, 5)
        self.pack_radio_buttons(vbox2, 'Sample Type')
        if attach == False:
            self.pack_spinbutton(vbox2, 'Run Time Before Sample (sec)')
        expander = gtk.Expander("Advanced")
        hbox = gtk.HBox()
        self.pack_spinbutton(hbox, 'Num Retries')
        self.pack_spinbutton(hbox, 'Retry Frequency (ms)')
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

    def pack_spinbutton(self, box, option):
        """Pack a spin button into the spcified box for the specified option."""
        hbox = gtk.HBox()
        label = gtk.Label(option)
        hbox.pack_start(label, False, False, 0)
        adj = gtk.Adjustment(1.0, 0.0, 1000000.0, 1.0, 100.0, 0.0)
        spinner = gtk.SpinButton(adj, 0, 0)
        spinner.set_value(self.options[option])
        hbox.pack_start(spinner, False, False, 0)
        box.pack_start(hbox, False, False, 10)
        self.spinners[option] = spinner

    def pack_combo_box(self, box, option):
        """Pack a combo box into the spcified box for the specified option."""
        hbox = gtk.HBox()
        hbox.pack_start(gtk.Label("Specify %s:" %option), False, False, 0)
        combo_box = gtk.combo_box_new_text()
        for type in self.types[option]:
            combo_box.append_text(type)
        combo_box.set_active(self.types[option].index(self.options[option]))
        hbox.pack_start(combo_box, False, False, 10)    
        self.combo_boxes[option] = combo_box
        box.pack_start(hbox, False, False, 0)

    def pack_check_button(self, box, option, center=False, expand=False, pad=0):
        """Pack a check button into the specified box for the specified option."""
        check_button = gtk.CheckButton(option)
        if self.options[option] == True:
            check_button.set_active(True)
        else:
            check_button.set_active(False)
        check_button.connect('toggled', lambda w: self.on_toggle_check_button(w, option))
        box.pack_start(check_button, center, expand, pad)

    def on_toggle_check_button(self, widget, option):
        """Callback to toggle on/off a check button."""
        if self.options[option] == True:
            self.options[option] = False
        else:            
            self.options[option] = True

    def pack_string_option(self, box, option, parent_window):
        """Pack a button into the specified box that generates a text entry dialog for the specified option."""
        hbox = gtk.HBox()
        label =  gtk.Label('%s: %s' %(option, self.options[option]))
        hbox.pack_start(label, False, False, 0)
        box.pack_start(hbox, False, False, 10)
        button = gtk.Button('Modify %s' %option)
        button.connect("clicked", lambda w: self.update_option(w, label, parent_window, option))
        box.pack_start(button, False, False, 0)

def STATGUI_main(argv):
    """The STATGUI main."""
    window = STATGUI()
    STATview.window = window
    window.connect('destroy', window.on_destroy)
    #gtk.gdk.threads_init()
    gtk.main()

if __name__ == '__main__':
    STATGUI_main(sys.argv)
