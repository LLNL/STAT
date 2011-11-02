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
#include "STAT.h"
#include "STAT_timer.h"
#include "graphlib.h"
#include "mrnet/MRNet.h"
#include "xplat/NetUtils.h"
#include "lmon_api/lmon_be.h"

#ifdef STACKWALKER
    #include "Symtab.h"
    #include "walker.h"
    #include "procstate.h"
    #include "frame.h"
    #include "swk_errors.h"
    #include "Type.h"
#ifndef BGL    
    #include "Variable.h"
    #include "Function.h"
#endif    
#ifdef PROTOTYPE    
    #include "local_var.h"
#endif    
    #include <sys/select.h>
    #include <errno.h>
#else
    #include "BPatch.h"
    #include "BPatch_Vector.h"
    #include "BPatch_thread.h"
    #include "BPatch_process.h"
    #include "BPatch_image.h"
    #include "BPatch_frame.h"
    #include "BPatch_function.h"
    #include "BPatch_snippet.h"
    #include "BPatch_statement.h"
#endif

#ifdef SBRS
    #include "sbrs_std.h"
#endif

#ifdef STAT_FGFS
    #include "Comm/MRNetCommFabric.h"
    #include "AsyncFastGlobalFileStat.h"
    #include "MRNetSymbolReader.h"
#endif

//! A struct that contains MRNet connection information to send to the daemons
typedef struct
{
    /* The char arrays are statically sized to make it easy to broadcast */
    char hostName[STAT_MAX_BUF_LEN];
    char parentHostName[STAT_MAX_BUF_LEN];
    int rank;
    int parentPort;
    int parentRank;
} statLeafInfo_t;

//! A struct to hold a list of statLeafInfo_t objects
typedef struct
{
    int size;
    statLeafInfo_t *leaves;
} statLeafInfoArray_t;

//! A struct to specify variables to gather
typedef struct
{
    char fileName[BUFSIZE];
    char variableName[BUFSIZE];
    int lineNum;
    int depth;
} statVariable_t;

//! Unpacks the MRNet parent node info for all daemons
/*!
    \param buf - the input buffer
    \param bufLen - the length of the input buffer
    \param[out] data - the return data 
    \return 0 on success
*/
int Unpack_STAT_BE_info(void *buf, int bufLen, void *data);

//! Generates an integer ID for a given string
/*!
    \param str - the input string
    \return - an unsigned integer that is likely to be unique to this string

    Tries to generate a unique ID for an inputted call path.
*/
unsigned int string_hash(const char *str);

//! STAT initialization code
/*!
    \param[in,out] argc - the number of arguments
    \param[in,out] argv - the argument list
    \return STAT_OK on success
*/
StatError_t statInit(int *argc, char ***argv);

//! STAT finalization code
/*!
    \return STAT_OK on success
*/
StatError_t statFinalize();

#ifdef SBRS
//! Broadcast function to register for SBRS
/*!
    \param buf - the buffer to broadcast
    \param size - the buffer size
    \return 0 on success
*/
int bcastWrapper(void *buf, int size);

//! Master check function to register for SBRS
/*!
    \return 1 if this daemon is the master, 0 otherwise
*/
int isMasterWrapper();
#endif

//! The STAT daemon object used to gather and send stack traces
class STAT_BackEnd
{
    public:
        //! Default constructor
        STAT_BackEnd();

        //! Default destructor
        ~STAT_BackEnd();

        /******************************/
        /* Core STAT BE functionality */
        /******************************/

        //! Initialize and set up LaunchMON
        /*
            \return STAT_OK on success
        */
        StatError_t Init();

        //! Connect to the MRNet tree
        /*!
            \return STAT_OK on success
        
            Receive the connection information from the frontend and broadcast it to all
            the daemons.  Call the MRNet Network constructor with this daemon's MRNet
            personality.
        */
        StatError_t Connect();

        //! Receives messages from FE and executes the requests
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
            \return STAT_OK on success
        */
        StatError_t startLog(char *logOutDir);

        //! Write MRNet connection information to a named fifo
        /*!
            \return STAT_OK on success
        
            Called by the helper daemon to write MRNet connection information for the
            STATBench daemon emulators to a fifo.
        */
        StatError_t statBenchConnectInfoDump();

        //! Connects to the MRNet tree
        /*!
            \return STAT_OK on success
        
            Reads in connection information from the named fifo and uses it to derrive
            this daemon emulator's MRNet personality, which is then passed to the MRNet
            Network constructor.
        */
        StatError_t statBenchConnect();

    private:
        //! Attach to the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t Attach();

        //! Pause the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t Pause();

        //! Resume the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t Resume();

#ifdef STACKWALKER        
        //! Pauses a single process
        /*!
            \param walker - the process to pause
            \return STAT_OK on success
        */
        StatError_t pauseImpl(Dyninst::Stackwalker::Walker *walker);

        //! Resumes a single process
        /*!
            \param walker - the process to resume
            \return STAT_OK on success
        */
        StatError_t resumeImpl(Dyninst::Stackwalker::Walker *walker);
#else
        //! Pauses a single process
        /*!
            \param proc - the process to pause
            \return STAT_OK on success
        */
        StatError_t pauseImpl(BPatch_process *proc);

        //! Resumes a single process
        /*!
            \param proc - the process to resume
            \return STAT_OK on success
        */
        StatError_t resumeImpl(BPatch_process *proc);
#endif      
        //! Gathers the specified number of traces from all processes
        /*!
            \param nTraces - the number of traces to gather per process
            \param traceFrequency - the time to wait between samples
            \param nRetries - the number of attempts to try to get a complete stack 
                trace
            \param retryFrequency - the time to wait between retries
            \param sampleType - the level of detail to gather in the trace
            \param withThreads - whether to gather thread stack traces too
            \param clearOnSample - whether to clear the accumulated traces before
                sampling
            \param variableSpecification - the specification of variables to extract
            \return STAT_OK on success
        */
        StatError_t sampleStackTraces(unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, unsigned int sampleType, unsigned int withThreads, unsigned int clearOnSample, char *variableSpecification);
#ifdef STACKWALKER
        //! Get a single stack trace
        /*!
            \param[out] retGraph - the return graph
            \param proc - the current process
            \param rank - the current process rank
            \param nRetries - the number of attempts to try to get a complete stack 
                trace
            \param retryFrequency - the time to wait between retries
            \param sampleType - the level of detail to gather in the trace
            \param withThreads - whether to gather thread stack traces too
            \param extractVariables - a list of variables to extract
            \param nVariables - the number of variables to extract
            \return STAT_OK on success
        */
        StatError_t getStackTrace(graphlib_graph_p retGraph, Dyninst::Stackwalker::Walker *proc, int rank, unsigned int nRetries, unsigned int retryFrequency, unsigned int sampleType, unsigned int withThreads, statVariable_t *extractVariables, int nVariables);

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
            \param swalk - the stack trace to extract from
            \param frame - the frame from which to gather the variable
            \param variableName - the name of the variable to gather
            \param[out] outBuf - the value of the variable
        */
        StatError_t getVariable(std::vector<Dyninst::Stackwalker::Frame> swalk, unsigned int frame, char *variableName, char *outBuf);
#else
        //! Get a single stack trace
        /*!
            \param[out] retGraph - the return graph
            \param proc - the current process
            \param rank - the current process rank
            \param nRetries - the number of attempts to try to get a complete stack 
                trace
            \param retryFrequency - the time to wait between retries
            \param sampleType - the level of detail to gather in the trace
            \param withThreads - whether to gather thread stack traces too
            \param extractVariables - a list of variables to extract
            \param nVariables - the number of variables to extract
            \return STAT_OK on success
        */
        StatError_t getStackTrace(graphlib_graph_p retGraph, BPatch_process *proc, int rank, unsigned int nRetries, unsigned int retryFrequency, unsigned int sampleType, unsigned int withThreads, statVariable_t *extractVariables, int nVariables);

        //! Translates an address into a source file and line number
        /*!
            \param func - the current process
            \param addr - the address to translate
            \param[out] outBuf - the source file name
            \param[out] lineNum - the line number
            \return STAT_OK on success
        */
        StatError_t getSourceLine(BPatch_function *func, unsigned long addr, char *outBuf, int *lineNum);

        //! Extract a variable value from the application
        /*!
            \param proc - the process to extract from
            \param functionName - the name of the function to gather the variable
            \param variableName - the name of the variable to gather
            \param[out] outBuf - the value of the variable
        */
        StatError_t getVariable(BPatch_process *proc, char *functionName, char *variableName, char *outBuf);
#endif
        //! Parse the variable specification
        /*!
            \param variableSpecification - the variable specification
            \param[out] outBuf - the output list of variables
            \param[out] nVariables - the number of variables
            \return STAT_OK on success
            
            Parse the variable specification to get individual variables and frame 
            references variableSpecification is of the form: 
        
            "num_elements#filename:line.depth$var[,filename:line.depth$var]*"
        
            i.e., "1#foo.C:1.2$i" or "2#foo.C:1.2$i,bar.C:3.4$j"
        */
        StatError_t parseVariableSpecification(char *variableSpecification, statVariable_t **outBuf, int *nVariables);

        //! Detaches from all of the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t Detach(unsigned int *stopArray, int stopArrayLen);

        //! Terminates all of the application processes
        /*!
            \return STAT_OK on success
        */
        StatError_t terminateApplication();

        //! Sends an acknowledgement to the front end
        /*!
            \param stream - the send stream
            \param tag - the packet tag
            \param val - the ack return value, 0 for success, 1 for failure
        */
        StatError_t sendAck(MRN::Stream *stream, int tag, int val);

        //! Creates stack traces
        /*!
            \param maxDepth - the maximum call path to generate
            \param nTasks - the number of tasks to emulate
            \param nTraces - the number of traces to generate per emulated task
            \param functionFanout - the max function fanout
            \param nEqClasses - the number of equivalence classes to generate
            \return STAT_OK on success
        */
        StatError_t statBenchCreateTraces(unsigned int maxDepth, unsigned int nTasks, unsigned int nTraces, unsigned int functionFanout, int nEqClasses);

        //! Creates a single stack trace
        /*!
            \param maxDepth - the maximum call path to generate
            \param task - the emulated task rank
            \param functionFanout - the max function fanout
            \param nEqClasses - the number of equivalence classes to generate
            \param iter - the iteration value to pass to the randomizer
            \return the generated graphlib graph
        */
        graphlib_graph_p statBenchCreateTrace(unsigned int maxDepth, unsigned int task, unsigned int functionFanout, int nEqClasses, unsigned int iter);

        /****************/
        /* Private data */
        /****************/

        int proctabSize_;                       /*!< the size of the process table */
        int processMapNonNull_;                 /*!< the number of active processes */
        char *parentHostName_;                  /*!< the hostname of the MRNet parent */
        char localHostName_[BUFSIZE];           /*!< the local hostname */
        char localIp_[BUFSIZE];                 /*!< the local IP address */
        FILE *errOutFp_;                        /*!< the error output file handle */
        FILE *logOutFp_;                        /*!< the log file handle */
        bool initialized_;                      /*!< whether LauncMON has been initialized */
        bool connected_;                        /*!< whether this daemon has been conected to MRNet */
        bool isRunning_;                        /*!< whether the target processes are running */
        MRN::Network *network_;                 /*!< the MRNet Network object */
        MRN::Rank myRank_;                      /*!< this daemon's MRNet rank */
        MRN::Rank parentRank_;                  /*!< the MRNet parent's rank */
        MRN::Port parentPort_;                  /*!< the MRNet parent's port */
        graphlib_graph_p prefixTree3d_;         /*!< the 3D prefix tree */
        graphlib_graph_p prefixTree2d_;         /*!< the 2D prefix tree */
        MPIR_PROCDESC_EXT *proctab_;            /*!< the process table */
#ifdef STACKWALKER
        std::map<int, Dyninst::Stackwalker::Walker *> processMap_;  /*!< the debug process objects */
#else
        BPatch bpatch_;                         /*!< the application bpatch object */
        std::map<int, BPatch_process*> processMap_; /*!< the debug process objects */
#endif
        MRN::Stream *broadcastStream_;          /*!< the main broadcast/acknowledgement stream */
#ifdef STAT_FGFS
        FastGlobalFileStat::CommLayer::CommFabric *fgfsCommFabric_;
#endif

};

#endif /* __STAT_BACKEND_H */
