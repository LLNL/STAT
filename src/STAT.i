/*
Copyright (c) 2007-2008, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee <lee218@llnl.gov>, Dorian Arnold, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-400455.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

%module STAT

/*%include "std_vector.i"
namespace std{
    %template(IntVector) vector<int>;
};*/

%{
#include "config.h"
%}

%{
#include "STAT_FrontEnd.h"
%}

%include "carrays.i"
%array_class(int, intArray)

#define STAT_UNKNOWN -1

enum StatLogOptions_t {
               STAT_LOG_NONE = 0x00,
               STAT_LOG_FE = 0x01,
               STAT_LOG_BE = 0x02,
               STAT_LOG_CP = 0x04,
               STAT_LOG_MRN = 0x08
} ;

typedef enum {
    STAT_FUNCTION_NAME_ONLY = 0,
    STAT_FUNCTION_AND_PC,
    STAT_FUNCTION_AND_LINE,
    STAT_CR_FUNCTION_NAME_ONLY,
    STAT_CR_FUNCTION_AND_PC,
    STAT_CR_FUNCTION_AND_LINE
} StatSample_t;

typedef enum {
    STAT_LAUNCH = 0,
    STAT_ATTACH
} StatLaunch_t;

typedef enum {
    STAT_VERBOSE_ERROR = 0,
    STAT_VERBOSE_STDOUT,
    STAT_VERBOSE_FULL
} StatVerbose_t;

typedef enum {
    STAT_TOPOLOGY_DEPTH = 0,
    STAT_TOPOLOGY_FANOUT,
    STAT_TOPOLOGY_USER,
    STAT_TOPOLOGY_AUTO
} StatTopology_t;

typedef enum {
               STAT_OK = 0,
               STAT_SYSTEM_ERROR,
               STAT_MRNET_ERROR,
               STAT_FILTERLOAD_ERROR,
               STAT_GRAPHLIB_ERROR,
               STAT_ALLOCATE_ERROR,
               STAT_ATTACH_ERROR,
               STAT_DETACH_ERROR,
               STAT_SEND_ERROR,
               STAT_SAMPLE_ERROR,
               STAT_TERMINATE_ERROR,
               STAT_FILE_ERROR,
               STAT_LMON_ERROR,
               STAT_ARG_ERROR,
               STAT_VERSION_ERROR,
               STAT_NOT_LAUNCHED_ERROR,
               STAT_NOT_ATTACHED_ERROR,
               STAT_NOT_CONNECTED_ERROR,
               STAT_NO_SAMPLES_ERROR,
               STAT_WARNING,
               STAT_LOG_MESSAGE,
               STAT_STDOUT,
               STAT_VERBOSITY,
               STAT_STACKWALKER_ERROR,
               STAT_PAUSE_ERROR,
               STAT_RESUME_ERROR,
               STAT_DAEMON_ERROR,
               STAT_APPLICATION_EXITED,
               STAT_PENDING_ACK
} StatError_t;

class STAT_FrontEnd
{
    public:
        STAT_FrontEnd();
        ~STAT_FrontEnd();

        StatError_t attachAndSpawnDaemons(unsigned int pid, char *remoteNode = NULL);
        StatError_t launchAndSpawnDaemons(char *remoteNode = NULL, bool isStatBench = false);
        StatError_t launchMrnetTree(StatTopology_t topologyType, char *topologySpecification, char *nodeList = NULL, bool blocking = true, bool shareAppNodes = false, bool isStatBench = false);
        StatError_t connectMrnetTree(bool blocking = true, bool isStatBench = false);
        StatError_t attachApplication(bool blocking = true);
        StatError_t pause(bool blocking = true);
        StatError_t resume(bool blocking = true);
        bool isRunning();
        StatError_t sampleStackTraces(StatSample_t sampleType, bool withThreads, bool withPython, bool clearOnSample, unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, bool blocking = true, char *variableSpecification = "NULL");
        StatError_t gatherLastTrace(bool blocking = true);
        StatError_t gatherTraces(bool blocking = true);
        char *getLastDotFilename();
        void shutDown();
        StatError_t detachApplication(bool blocking = true);
        StatError_t detachApplication(int *stopList, int stopListSize, bool blocking = true);
        StatError_t terminateApplication(bool blocking = true);
        void printMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...);
        StatError_t startLog(unsigned char logType, char *logOutDir);
        StatError_t receiveAck(bool blocking = true);
        StatError_t statBenchCreateStackTraces(unsigned int maxDepth, unsigned int nTasks, unsigned int nTraces, unsigned int functionFanout, int nEqClasses, int iCountRep);
        char *getNodeInEdge(int nodeId);

        void printCommunicationNodeList();
        void printApplicationNodeList();
        unsigned int getLauncherPid();
        unsigned int getNumApplProcs();
        unsigned int getNumApplNodes();
        void setJobId(unsigned int jobId);
        unsigned int getJobId();
        const char *getApplExe();
        StatError_t setToolDaemonExe(const char *toolDaemonExe);
        const char *getToolDaemonExe();
        StatError_t setOutDir(const char *outDir);
        const char *getOutDir();
        StatError_t setFilePrefix(const char *filePrefix);
        const char *getFilePrefix();
        void setProcsPerNode(unsigned int procsPerNode);
        unsigned int getProcsPerNode();
        StatError_t setFilterPath(const char *filterPath);
        const char *getFilterPath();
        const char *getRemoteNode();
        StatError_t addLauncherArgv(const char *launcherArg);
        const char **getLauncherArgv();
        unsigned int getLauncherArgc();
        void setVerbose(StatVerbose_t verbose);
        StatVerbose_t getVerbose();
        char *getLastErrorMessage();
        StatError_t addPerfData(const char *buf, double time);
        const char *getInstallPrefix();
        void getVersion(int *version);
};

