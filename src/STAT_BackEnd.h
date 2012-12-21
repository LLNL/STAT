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

#ifndef __STAT_BACKEND_H
#define __STAT_BACKEND_H

#define STAT_MAX_BUF_LEN 256

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <arpa/inet.h>
#include <set>
#include <sys/select.h>
#include <errno.h>

#include "STAT.h"
#include "STAT_timer.h"
#include "STAT_GraphRoutines.h"
#include "graphlib.h"
#include "lmon_api/lmon_be.h"

#include "Symtab.h"
#include "walker.h"
#include "procstate.h"
#include "frame.h"
#include "swk_errors.h"
#include "Type.h"
#ifdef SW_VERSION_8_0_0
  #include "PlatFeatures.h"
  #include "ProcessSet.h"
  #include "PCErrors.h"
//  #if !defined(PROTOTYPE_PY) && !defined(PROTOTYPE_TO) && defined(BGL)
    #define GROUP_OPS
//  #endif
#endif
#if defined(PROTOTYPE_TO) || defined(PROTOTYPE_PY)
  #include "local_var.h"
  #include "Variable.h"
  #include "Function.h"
#endif

#ifdef STAT_FGFS
  #include "Comm/MRNetCommFabric.h"
  #include "AsyncFastGlobalFileStat.h"
  #include "MRNetSymbolReader.h"
#endif

//! An enum type to determine who launched the daemon
typedef enum {
    STATD_LMON_LAUNCH = 0,
    STATD_SERIAL_LAUNCH,
    STATD_MRNET_LAUNCH,
} StatDaemonLaunch_t;

//! A struct that contains MRNet connection information to send to the daemons
typedef struct
{
    /* The char arrays are statically sized to make it easy to broadcast */
    char hostName[STAT_MAX_BUF_LEN];
    char parentHostName[STAT_MAX_BUF_LEN];
    int rank;
    int parentPort;
    int parentRank;
} StatLeafInfo_t;

//! A struct to hold a list of StatLeafInfo_t objects
typedef struct
{
    int size;
    StatLeafInfo_t *leaves;
} StatLeafInfoArray_t;

//! A struct to specify variables to gather
typedef struct
{
    char fileName[BUFSIZE];
    char variableName[BUFSIZE];
    int lineNum;
    int depth;
} StatVariable_t;

//! Unpacks the MRNet parent node info for all daemons
/*!
    \param buf - the input buffer
    \param bufLen - the length of the input buffer
    \param[out] data - the return data
    \return 0 on success
*/
int unpackStatBeInfo(void *buf, int bufLen, void *data);

//! STAT initialization code
/*!
    \param[in,out] argc - the number of arguments
    \param[in,out] argv - the argument list
    \param[in] launchType - the launch type (i.e., LMON or MRNet)
    \return STAT_OK on success
*/
StatError_t statInit(int *argc, char ***argv, StatDaemonLaunch_t launchType = STATD_LMON_LAUNCH);

//! STAT finalization code
/*!
    \param[in] launchType - the launch type (i.e., LMON or MRNet)
    \return STAT_OK on success
*/
StatError_t statFinalize(StatDaemonLaunch_t launchType = STATD_LMON_LAUNCH);

//! The STAT daemon object used to gather and send stack traces
class STAT_BackEnd
{
    public:
        //! Default constructor
        STAT_BackEnd(StatDaemonLaunch_t launchType);

        //! Default destructor
        ~STAT_BackEnd();

        /******************************/
        /* Core STAT BE functionality */
        /******************************/

        //! Initialize and set up LaunchMON
        /*
            \return STAT_OK on success
        */
        StatError_t init();

        //! Add a serial process to the process table
        /*
            \param pidString - the input exe@hostname:pid string
            \return STAT_OK on success
        */
        StatError_t addSerialProcess(const char *pidString);

        //! Connect to the MRNet tree
        /*!
            \param argc - [optional] the arg count to pass to MRNet
            \param argv - [optional] the arg list to pass to MRNet
            \return STAT_OK on success

            Receive the connection information from the frontend and broadcast
            it to all the daemons.  Call the MRNet Network constructor with 
            this daemon's MRNet personality.
        */
        StatError_t connect(int argc = 0, char **argv = NULL);

        //! Receive messages from FE and execute the requests
        /*!
            \return STAT_OK on success

            Loops on MRNet receive and executes the requested command
        */
        StatError_t mainLoop();

        //! Print the STAT error type and error message
        /*!
            \param statError - the STAT error type
            \param sourceFile - the source file where the error was reported
            \param sourceLine - the source line where the error was reported
            \param fmt - the output format string
            \param ... - any variables to output
        */
        void printMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...);

        //! Creates the log file
        /*!
            \param logOutDir - the output log directory
            \param useMrnetPrintf - [optional] whether to use MRNet's printf for STAT logging
            \param mrnetOutputLevel - [optional] the MRNet logging output level
            \return STAT_OK on success
        */
        StatError_t startLog(char *logOutDir, bool useMrnetPrintf = false, int mrnetOutputLevel = 1);

        //! Write MRNet connection information to a named fifo
        /*!
            \return STAT_OK on success

            Called by the helper daemon to write MRNet connection information 
            for the STATBench daemon emulators to a fifo.
        */
        StatError_t statBenchConnectInfoDump();

        //! Connects to the MRNet tree
        /*!
            \return STAT_OK on success

            Reads in connection information from the named fifo and uses it to
            derrive this daemon emulator's MRNet personality, which is then 
            passed to the MRNet Network constructor.
        */
        StatError_t statBenchConnect();

    private:
        //! Attach to the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t attach();

        //! Pause the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t pause();

        //! Resume the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t resume();

        //! Pause a single process
        /*!
            \param walker - the process to pause
            \return STAT_OK on success
        */
        StatError_t pauseImpl(Dyninst::Stackwalker::Walker *walker);

        //! Resume a single process
        /*!
            \param walker - the process to resume
            \return STAT_OK on success
        */
        StatError_t resumeImpl(Dyninst::Stackwalker::Walker *walker);

        //! Gather the specified number of traces from all processes
        /*!
            \param nTraces - the number of traces to gather per process
            \param traceFrequency - the time (ms) to wait between samples
            \param nRetries - the number of attempts to try to get a complete 
                   stack trace
            \param retryFrequency - the time (us) to wait between retries
            \param withThreads - whether to gather thread stack traces too
            \param clearOnSample - whether to clear the accumulated traces
                   before sampling
            \param variableSpecification - the specification of variables to
                   extract
            \param withPython - whether to dive into the Python interpreter to
                   get Python-level traces
            \return STAT_OK on success
        */
        StatError_t sampleStackTraces(unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads, unsigned int clearOnSample, char *variableSpecification, unsigned int withPython);

        //! Merge the given graph into the 2d and 3d graphs
        /*!
            \param currentGraph - the graph to merge
            \param isLastTrace - true if this graph is from the last iteration
                   of stackwalks (and thus should be merged into the 2d graph)
            \return STAT_OK on success
        */
        StatError_t mergeIntoGraphs(graphlib_graph_p currentGraph, bool isLastTrace);

#if defined GROUP_OPS
        //! Get a stack trace from every process
        /*!
            \param[out] retGraph - the return graph
            \param nRetries - the number of attempts to try to get a complete
                   stack trace
            \param retryFrequency - the time to wait between retries
            \param withThreads - whether to gather thread stack traces
            \return STAT_OK on success
        */
        StatError_t getStackTraceFromAll(graphlib_graph_p retGraph, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads);

        //! Add a frame to a given graph
        /*!
            \param[in,out] graphlibGraph - the graph object to add the frame to
            \param stackwalkerGraph - the current Stackwalker graph object
            \param graphlibNode - the node to append to
            \param stackwalkerNode - the current node in the Stackwalker graph
            \param nodeIdNames - the concatenated string of node IDs
            \param[out] errorThreads - a list of threads whose stack walks
                   ended in an error
            \param[out] outputRanks - the set of ranks in this stack walk
            \return STAT_OK on success
        */
        StatError_t addFrameToGraph(graphlib_graph_p graphlibGraph, Dyninst::Stackwalker::CallTree *stackwalkerGraph, graphlib_node_t graphlibNode, Dyninst::Stackwalker::FrameNode *stackwalkerNode, std::string nodeIdNames, std::set<std::pair<Dyninst::Stackwalker::Walker *, Dyninst::THR_ID> > *errorThreads, std::set<int> &outputRanks);
#endif

        //! Get a single stack trace
        /*!
            \param[out] retGraph - the return graph
            \param proc - the current process
            \param rank - the current process rank
            \param nRetries - the number of attempts to try to get a complete
                   stack trace
            \param retryFrequency - the time to wait between retries
            \param withThreads - whether to gather thread stack traces
            \param withPython - whether to dive into the Python interpreter to
                   get Python script-level traces
            \return STAT_OK on success
        */
        StatError_t getStackTrace(graphlib_graph_p retGraph, Dyninst::Stackwalker::Walker *proc, int rank, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads, unsigned int withPython);

        //! Translates an address into a source file and line number
        /*!
            \param proc - the current process
            \param addr - the address to translate
            \param[out] outBuf - the source file name
            \param[out] lineNum - the line number
            \return STAT_OK on success
        */
        StatError_t getSourceLine(Dyninst::Stackwalker::Walker *proc, Dyninst::Address addr, char *outBuf, int *lineNum);

        //! Extract a variable value from the application
        /*!
            \param frame - the frame from which to gather the variable
            \param variableName - the name of the variable to gather
            \param[out] outBuf - the value of the variable
        */
        StatError_t getVariable(const Dyninst::Stackwalker::Frame &frame, char *variableName, char *outBuf, unsigned int outBufSize);

        //! Return the STAT name that should be given for a specific frame
        /*!
            \param frame - the Frame to gather the name from
            \param depth - [optional] the depth of this frame in the stack walk. This is necessary for identifying the appropriate frame for variable extraction.
            \return the name to use for this frame
        */
        public:
        std::string getFrameName(const Dyninst::Stackwalker::Frame &frame, int depth = -1);
        private:

        //! Initialize the nodeAttr's name field with the given string
        /*!
          \param name - the name to use
          \param nodeAttr[in,out] - the graphlib_nodeaddr_t whose name we should change
        */
        void fillInName(const std::string &name, graphlib_nodeattr_t &nodeAttr);

        //! Parse the variable specification
        /*!
            \param variableSpecification - the variable specification
            \return STAT_OK on success

            Parse the variable specification to get individual variables and
            frame references variableSpecification is of the form:

            "num_elements#filename:line.depth$var[,filename:line.depth$var]*"

            i.e., "1#foo.C:1.2$i" or "2#foo.C:1.2$i,bar.C:3.4$j"
        */
        StatError_t parseVariableSpecification(char *variableSpecification);

        //! Detach from all of the application processes
        /*!
            \param stopArray - array of process ranks to leave in stopped state
            \param - stopArrayLen - the length of the stop array
            \return STAT_OK on success
        */
        StatError_t detach(unsigned int *stopArray, int stopArrayLen);

        //! Terminate all of the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t terminateApplication();

        //! Send an acknowledgement to the front end
        /*!
            \param stream - the send stream
            \param tag - the packet tag
            \param val - the ack return value, 0 for success, 1 for failure
        */
        StatError_t sendAck(MRN::Stream *stream, int tag, int val);

        //! Get a struct's components for a given type from Symtab
        /*!
            \param type - the struct type
            \return the vector of the struct's components on success
        */
        std::vector<Dyninst::SymtabAPI::Field *> *getComponents(Dyninst::SymtabAPI::Type *type);

        //! Get Python script level frame info
        /*!
            \param proc - the Walker object for this process
            \param swalk - the currnet stack frames
            \param frame - the index for the current frame
            \param [out] pyFun - the Python function name
            \param [out] pySource - the Python source file
            \param [out] pyLineNo - the current Python line number
            \return STAT_OK on success
        */
        StatError_t getPythonFrameInfo(Dyninst::Stackwalker::Walker *proc, const Stackwalker::Frame &frame, char **pyFun, char **pySource, int *pyLineNo);

        //! Create stack traces
        /*!
            \param maxDepth - the maximum call path to generate
            \param nTasks - the number of tasks to emulate
            \param nTraces - the number of traces to generate per emulated task
            \param functionFanout - the max function fanout
            \param nEqClasses - the number of equivalence classes to generate
            \return STAT_OK on success
        */
        StatError_t statBenchCreateTraces(unsigned int maxDepth, unsigned int nTasks, unsigned int nTraces, unsigned int functionFanout, int nEqClasses);

        //! Create a single stack trace
        /*!
            \param maxDepth - the maximum call path to generate
            \param task - the emulated task rank
            \param nTasks - the total number of emulated tasks
            \param functionFanout - the max function fanout
            \param nEqClasses - the number of equivalence classes to generate
            \param iter - the iteration value to pass to the randomizer
            \return the generated graphlib graph
        */
        graphlib_graph_p statBenchCreateTrace(unsigned int maxDepth, unsigned int task, unsigned int nTasks, unsigned int functionFanout, int nEqClasses, unsigned int iter);

        /****************/
        /* Private data */
        /****************/

        int proctabSize_;               /*!< the size of the process table */
        int processMapNonNull_;         /*!< the number of active processes */
        char *parentHostName_;          /*!< the hostname of the MRNet parent */
        char localHostName_[BUFSIZE];   /*!< the local hostname */
        char localIp_[BUFSIZE];         /*!< the local IP address */
        FILE *errOutFp_;                /*!< the error output file handle */
        bool initialized_;              /*!< whether STAT has been initialized */
        bool connected_;                /*!< whether this daemon has been conected to MRNet */
        bool isRunning_;                /*!< whether the target processes are running */
        bool useMrnetPrintf_;           /*!< whether to use MRNet's printf for logging */
        bool doGroupOps_;               /*!< do group operations through StackwalkerAPI */
        bool withPython_; //TODO
        MRN::Network *network_;         /*!< the MRNet Network object */
        MRN::Rank myRank_;              /*!< this daemon's MRNet rank */
        MRN::Rank parentRank_;          /*!< the MRNet parent's rank */
        MRN::Port parentPort_;          /*!< the MRNet parent's port */
        MRN::Stream *broadcastStream_;  /*!< the main broadcast/acknowledgement stream */
        graphlib_graph_p prefixTree3d_; /*!< the 3D prefix tree */
        graphlib_graph_p prefixTree2d_; /*!< the 2D prefix tree */
        MPIR_PROCDESC_EXT *proctab_;    /*!< the process table */
        StatDaemonLaunch_t launchType_; /*!< the launch type */
        StatSample_t sampleType_;       /*!< type of sample we're currently collecting */
        int nVariables_;                /*!< the number of variables to extract */
        StatVariable_t *extractVariables_;  /*!< a list of variables to extract for the current sample */

        std::map<int, StatBitVectorEdge_t *> nodeInEdgeMap_;          /*!< a record of edge labels */
        std::map<int, Dyninst::Stackwalker::Walker *> processMap_;      /*!< the debug process objects */
        std::map<Dyninst::Stackwalker::Walker *, int> procsToRanks_;    /*!< translate a process into a rank */


#ifdef GROUP_OPS
        Dyninst::ProcControlAPI::ProcessSet::ptr procSet_;
        Dyninst::Stackwalker::WalkerSet *walkerSet_;
#endif

#ifdef STAT_FGFS
        FastGlobalFileStatus::CommLayer::CommFabric *fgfsCommFabric_;
#endif

};

#endif /* __STAT_BACKEND_H */
