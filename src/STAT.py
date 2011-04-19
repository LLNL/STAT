# This file was automatically generated by SWIG (http://www.swig.org).
# Version 2.0.1
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.
# This file is compatible with both classic and new-style classes.

from sys import version_info
if version_info >= (2,6,0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_STAT', [dirname(__file__)])
        except ImportError:
            import _STAT
            return _STAT
        if fp is not None:
            try:
                _mod = imp.load_module('_STAT', fp, pathname, description)
            finally:
                fp.close()
            return _mod
    _STAT = swig_import_helper()
    del swig_import_helper
else:
    import _STAT
del version_info
try:
    _swig_property = property
except NameError:
    pass # Python < 2.2 doesn't have 'property'.
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "thisown"): return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    if (name == "thisown"): return self.this.own()
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError(name)

def _swig_repr(self):
    try: strthis = "proxy of " + self.this.__repr__()
    except: strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

try:
    _object = object
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0


class intArray(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, intArray, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, intArray, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        this = _STAT.new_intArray(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _STAT.delete_intArray
    __del__ = lambda self : None;
    def __getitem__(self, *args): return _STAT.intArray___getitem__(self, *args)
    def __setitem__(self, *args): return _STAT.intArray___setitem__(self, *args)
    def cast(self): return _STAT.intArray_cast(self)
    __swig_getmethods__["frompointer"] = lambda x: _STAT.intArray_frompointer
    if _newclass:frompointer = staticmethod(_STAT.intArray_frompointer)
intArray_swigregister = _STAT.intArray_swigregister
intArray_swigregister(intArray)

def intArray_frompointer(*args):
  return _STAT.intArray_frompointer(*args)
intArray_frompointer = _STAT.intArray_frompointer

STAT_UNKNOWN = _STAT.STAT_UNKNOWN
STAT_LOG_FE = _STAT.STAT_LOG_FE
STAT_LOG_BE = _STAT.STAT_LOG_BE
STAT_LOG_ALL = _STAT.STAT_LOG_ALL
STAT_LOG_NONE = _STAT.STAT_LOG_NONE
STAT_FUNCTION_NAME_ONLY = _STAT.STAT_FUNCTION_NAME_ONLY
STAT_FUNCTION_AND_PC = _STAT.STAT_FUNCTION_AND_PC
STAT_FUNCTION_AND_LINE = _STAT.STAT_FUNCTION_AND_LINE
STAT_LAUNCH = _STAT.STAT_LAUNCH
STAT_ATTACH = _STAT.STAT_ATTACH
STAT_VERBOSE_ERROR = _STAT.STAT_VERBOSE_ERROR
STAT_VERBOSE_STDOUT = _STAT.STAT_VERBOSE_STDOUT
STAT_VERBOSE_FULL = _STAT.STAT_VERBOSE_FULL
STAT_TOPOLOGY_DEPTH = _STAT.STAT_TOPOLOGY_DEPTH
STAT_TOPOLOGY_FANOUT = _STAT.STAT_TOPOLOGY_FANOUT
STAT_TOPOLOGY_USER = _STAT.STAT_TOPOLOGY_USER
STAT_TOPOLOGY_AUTO = _STAT.STAT_TOPOLOGY_AUTO
STAT_OK = _STAT.STAT_OK
STAT_SYSTEM_ERROR = _STAT.STAT_SYSTEM_ERROR
STAT_MRNET_ERROR = _STAT.STAT_MRNET_ERROR
STAT_FILTERLOAD_ERROR = _STAT.STAT_FILTERLOAD_ERROR
STAT_GRAPHLIB_ERROR = _STAT.STAT_GRAPHLIB_ERROR
STAT_ALLOCATE_ERROR = _STAT.STAT_ALLOCATE_ERROR
STAT_ATTACH_ERROR = _STAT.STAT_ATTACH_ERROR
STAT_DETACH_ERROR = _STAT.STAT_DETACH_ERROR
STAT_SEND_ERROR = _STAT.STAT_SEND_ERROR
STAT_SAMPLE_ERROR = _STAT.STAT_SAMPLE_ERROR
STAT_TERMINATE_ERROR = _STAT.STAT_TERMINATE_ERROR
STAT_FILE_ERROR = _STAT.STAT_FILE_ERROR
STAT_LMON_ERROR = _STAT.STAT_LMON_ERROR
STAT_ARG_ERROR = _STAT.STAT_ARG_ERROR
STAT_VERSION_ERROR = _STAT.STAT_VERSION_ERROR
STAT_NOT_LAUNCHED_ERROR = _STAT.STAT_NOT_LAUNCHED_ERROR
STAT_NOT_ATTACHED_ERROR = _STAT.STAT_NOT_ATTACHED_ERROR
STAT_NOT_CONNECTED_ERROR = _STAT.STAT_NOT_CONNECTED_ERROR
STAT_NO_SAMPLES_ERROR = _STAT.STAT_NO_SAMPLES_ERROR
STAT_WARNING = _STAT.STAT_WARNING
STAT_LOG_MESSAGE = _STAT.STAT_LOG_MESSAGE
STAT_STDOUT = _STAT.STAT_STDOUT
STAT_VERBOSITY = _STAT.STAT_VERBOSITY
STAT_STACKWALKER_ERROR = _STAT.STAT_STACKWALKER_ERROR
STAT_PAUSE_ERROR = _STAT.STAT_PAUSE_ERROR
STAT_RESUME_ERROR = _STAT.STAT_RESUME_ERROR
STAT_DAEMON_ERROR = _STAT.STAT_DAEMON_ERROR
STAT_APPLICATION_EXITED = _STAT.STAT_APPLICATION_EXITED
STAT_PENDING_ACK = _STAT.STAT_PENDING_ACK
class STAT_FrontEnd(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, STAT_FrontEnd, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, STAT_FrontEnd, name)
    __repr__ = _swig_repr
    def __init__(self): 
        this = _STAT.new_STAT_FrontEnd()
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _STAT.delete_STAT_FrontEnd
    __del__ = lambda self : None;
    def attachAndSpawnDaemons(self, *args): return _STAT.STAT_FrontEnd_attachAndSpawnDaemons(self, *args)
    def launchAndSpawnDaemons(self, remoteNode = None, isStatBench = False): return _STAT.STAT_FrontEnd_launchAndSpawnDaemons(self, remoteNode, isStatBench)
    def launchMrnetTree(self, *args): return _STAT.STAT_FrontEnd_launchMrnetTree(self, *args)
    def connectMrnetTree(self, blocking = True, isStatBench = False): return _STAT.STAT_FrontEnd_connectMrnetTree(self, blocking, isStatBench)
    def attachApplication(self, blocking = True): return _STAT.STAT_FrontEnd_attachApplication(self, blocking)
    def pause(self, blocking = True): return _STAT.STAT_FrontEnd_pause(self, blocking)
    def resume(self, blocking = True): return _STAT.STAT_FrontEnd_resume(self, blocking)
    def isRunning(self): return _STAT.STAT_FrontEnd_isRunning(self)
    def sampleStackTraces(self, *args): return _STAT.STAT_FrontEnd_sampleStackTraces(self, *args)
    def gatherLastTrace(self, blocking = True): return _STAT.STAT_FrontEnd_gatherLastTrace(self, blocking)
    def gatherTraces(self, blocking = True): return _STAT.STAT_FrontEnd_gatherTraces(self, blocking)
    def getLastDotFilename(self): return _STAT.STAT_FrontEnd_getLastDotFilename(self)
    def shutDown(self): return _STAT.STAT_FrontEnd_shutDown(self)
    def detachApplication(self, *args): return _STAT.STAT_FrontEnd_detachApplication(self, *args)
    def terminateApplication(self, blocking = True): return _STAT.STAT_FrontEnd_terminateApplication(self, blocking)
    def printMsg(self, *args): return _STAT.STAT_FrontEnd_printMsg(self, *args)
    def startLog(self, *args): return _STAT.STAT_FrontEnd_startLog(self, *args)
    def receiveAck(self, blocking = True): return _STAT.STAT_FrontEnd_receiveAck(self, blocking)
    def statBenchCreateStackTraces(self, *args): return _STAT.STAT_FrontEnd_statBenchCreateStackTraces(self, *args)
    def printCommunicationNodeList(self): return _STAT.STAT_FrontEnd_printCommunicationNodeList(self)
    def printApplicationNodeList(self): return _STAT.STAT_FrontEnd_printApplicationNodeList(self)
    def getLauncherPid(self): return _STAT.STAT_FrontEnd_getLauncherPid(self)
    def getNumApplProcs(self): return _STAT.STAT_FrontEnd_getNumApplProcs(self)
    def getNumApplNodes(self): return _STAT.STAT_FrontEnd_getNumApplNodes(self)
    def setJobId(self, *args): return _STAT.STAT_FrontEnd_setJobId(self, *args)
    def getJobId(self): return _STAT.STAT_FrontEnd_getJobId(self)
    def getApplExe(self): return _STAT.STAT_FrontEnd_getApplExe(self)
    def setToolDaemonExe(self, *args): return _STAT.STAT_FrontEnd_setToolDaemonExe(self, *args)
    def getToolDaemonExe(self): return _STAT.STAT_FrontEnd_getToolDaemonExe(self)
    def setOutDir(self, *args): return _STAT.STAT_FrontEnd_setOutDir(self, *args)
    def getOutDir(self): return _STAT.STAT_FrontEnd_getOutDir(self)
    def setFilePrefix(self, *args): return _STAT.STAT_FrontEnd_setFilePrefix(self, *args)
    def getFilePrefix(self): return _STAT.STAT_FrontEnd_getFilePrefix(self)
    def setProcsPerNode(self, *args): return _STAT.STAT_FrontEnd_setProcsPerNode(self, *args)
    def getProcsPerNode(self): return _STAT.STAT_FrontEnd_getProcsPerNode(self)
    def setFilterPath(self, *args): return _STAT.STAT_FrontEnd_setFilterPath(self, *args)
    def getFilterPath(self): return _STAT.STAT_FrontEnd_getFilterPath(self)
    def getRemoteNode(self): return _STAT.STAT_FrontEnd_getRemoteNode(self)
    def addLauncherArgv(self, *args): return _STAT.STAT_FrontEnd_addLauncherArgv(self, *args)
    def getLauncherArgv(self): return _STAT.STAT_FrontEnd_getLauncherArgv(self)
    def getLauncherArgc(self): return _STAT.STAT_FrontEnd_getLauncherArgc(self)
    def setVerbose(self, *args): return _STAT.STAT_FrontEnd_setVerbose(self, *args)
    def getVerbose(self): return _STAT.STAT_FrontEnd_getVerbose(self)
    def getLastErrorMessage(self): return _STAT.STAT_FrontEnd_getLastErrorMessage(self)
    def addPerfData(self, *args): return _STAT.STAT_FrontEnd_addPerfData(self, *args)
    def getInstallPrefix(self): return _STAT.STAT_FrontEnd_getInstallPrefix(self)
    def getVersion(self, *args): return _STAT.STAT_FrontEnd_getVersion(self, *args)
STAT_FrontEnd_swigregister = _STAT.STAT_FrontEnd_swigregister
STAT_FrontEnd_swigregister(STAT_FrontEnd)



