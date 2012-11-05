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

#ifndef __STAT_FRONTEND_H
#define __STAT_FRONTEND_H


#include <string>
#include <list>
#include <set>
#include <map>
#include <cmath>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <pwd.h>
#include <pthread.h>
#include <sys/resource.h>
#include "arpa/inet.h"

#include "STAT.h"
#include "graphlib.h"
#ifdef GRAPHLIB20
#include "STAT_GraphRoutines.h"
#endif
#include "STAT_timer.h"
#include "lmon_api/lmon_fe.h"

#ifdef STAT_FGFS
    #include "Comm/MRNetCommFabric.h"
    #include "AsyncFastGlobalFileStat.h"
    #include "MountPointAttr.h"
    #include <fcntl.h>
#endif

#ifdef CRAYXT
  #ifndef MRNET31
extern "C"
{
    extern char *alpsGetMyNid(int32_t *);
    extern uint64_t alps_get_apid(int, int);
}
  #endif
#endif
#define STAT_MAX_FILENAME_ID 8192
#define STAT_MAX_FANOUT 64


//! An enum for STAT launch vs attach
typedef enum {
    STAT_LAUNCH = 0,
    STAT_ATTACH
} StatLaunch_t;

//! An enum for MRNet topology specification type
typedef enum {
    STAT_TOPOLOGY_DEPTH = 0,
    STAT_TOPOLOGY_FANOUT,
    STAT_TOPOLOGY_USER,
    STAT_TOPOLOGY_AUTO
} StatTopology_t;

//! A struct that contains MRNet connection information to send to the daemons
typedef struct
{
    MRN::NetworkTopology *networkTopology;
    std::multiset<std::string> daemons;
    std::vector<MRN::NetworkTopology::Node *> leafCps;
} LeafInfo_t;

//! A struct to help determine ranks lists for each daemon
typedef struct
{
    int count;
    int *list;
} IntList_t;

//! A struct to help devise the bit vector remap ordering
typedef struct _remap_node
{
    int lowRank;
    int numChildren;
    struct _remap_node **children;
} RemapNode_t;

//! The statPack function registered to LMON to send data to the daemons
/*!
    \param data - the input data
    \param[out] buf - the output buffer
    \param bufMax - the maximum buffer size
    \param[out] bufLen - the resulting buffer length
    \return 0 on success, -1 on error

    Parses "data" containing MRNet connection information to produce a buffer
    "buf" suitable for sending to the daemons.
*/
int statPack(void *data, void *buf, int bufMax, int *bufLen);

//! A callback function to detect daemon exit
/*!
    \param status - the input status vector
    \return 0 on success, -1 on error

    Determine if STAT is still connected to the resource manager
    and if the application is still running.
*/
int lmonStatusCb(int *status);

#if (defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT))
//! Increase nofile and nproc limits to maximum
StatError_t increaseSysLimits();
#endif

#ifdef MRNET3
//! Callback for MRNet BE connections
/*!
    \param event - the event type
    \param dummy - a dummy argument

    Records the number of MRNet BE connection events.
*/
void beConnectCb(MRN::Event *event, void *dummy);

//! Callback for daemon or CP exit
/*!
    \param event - the event type
    \param dummy - a dummy argument

    Handle the exiting of a node in the tree, reconfigure the bit vectors
*/
void nodeRemovedCb(MRN::Event *event, void *dummy);

//! Callback for topology modification
/*!
    \param event - the event type
    \param dummy - a dummy argument

    Handle the change of the tree topology, reconfigure the bit vectors
*/
void topologyChangeCb(MRN::Event *event, void *dummy);
#endif

//! The STAT FrontEnd object is used to Launch STAT daemons and gather and merge stack traces
class STAT_FrontEnd
{
    public:
        //! Default constructor
        /*!
            Sets trace values to defaults and required variables to unset values
        */
        STAT_FrontEnd();

        //! Default destructor.
        /*!
            First dumps any performnace data then frees up allocated variables.
        */
        ~STAT_FrontEnd();

        /***************************/
        /* STAT core functionality */
        /***************************/

        //! Attach to job launcher and launch the tool daemons
        /*!
            \param pid - the job launcher PID
            \param remoteNode - [optional] the remote host running the job launcher
            \return STAT_OK on success
        */
        StatError_t attachAndSpawnDaemons(unsigned int pid, char *remoteNode = NULL);

        //! Launch the application and launch the tool daemons
        /*!
            \param remoteNode - [optional] the remote host to run the job launcher
            \param isStatBench - [optional] true if running as STATBench
            \return STAT_OK on success
        */
        StatError_t launchAndSpawnDaemons(char *remoteNode = NULL, bool isStatBench = false);

        //! Launch the MRNet tree
        /*!
            \param topologyType - the requested topology type
            \param topologySpecification - the requested topology specification
            \param nodeList - [optional] the list of nodes for communication processes
            \param blocking - [optional] set to true if blocking on all connections set
            \param isStatBench - [optional] set to true if runninig as STATBench
            \return STAT_OK on success

            Creates the topology file, calls the Network constructor, and sends connection
            info to the daemons.  Waits for all daemons to connect if blocking set to true.
        */
        StatError_t launchMrnetTree(StatTopology_t topologyType, char *topologySpecification, char *nodeList = NULL, bool blocking = true, bool shareAppNodes = false, bool isStatBench = false);

        //! Connect the MRNet tree
        /*!
            \param blocking - [optional] set to true if blocking
            \param isStatBench - [optional] set to true if runninig as STATBench
            \return STAT_OK on success

            Receives BE connections.  When all BEs connected, creates the default streams
            and load the STAT filter.  Finally, checks all STAT components to make sure
            they are the same version.
        */
        StatError_t connectMrnetTree(bool blocking = true, bool isStatBench = false);

        //! Attach to the application
        /*!
            \param blocking - [optional] set to true if blocks until all BE acks received
            \return STAT_OK on success

            Broadcast a message to the daemons to attach and await all acks
        */
        StatError_t attachApplication(bool blocking = true);

        //! Pause the application
        /*!
            \param blocking - [optional] set to true if blocks until all BE acks received
            \return STAT_OK on success

            Broadcast a message to the daemons to pause and await all acks
        */
        StatError_t pause(bool blocking = true);

        //! Resume the application
        /*!
            \param blocking - [optional] set to true if blocks until all BE acks received
            \return STAT_OK on success

            Broadcast a message to the daemons to resume and await all acks
        */
        StatError_t resume(bool blocking = true);

        //! Check to see if we're running
        /*!
            \return true if application is running, false otherwise
        */
        bool isRunning();

        //! Broadcast a message to daemons to gather traces and await all acks
        /*!
            \param sampleType - the level of detail to sample
            \param withThreads - whether to gather samples from helper threads
            \param clearOnSample - whether to clear accumulated traces before sampling
            \param nTraces - the number of traces to gather per task
            \param traceFrequency - the amount of time to wait between samples
            \param nRetries - the number of times to attempt to gather a good trace
            \param retryFrequency - the amount of time to wait between retries
            \param blocking - [optional] set to true if blocks until all BE acks received
            \param variableSpecification - a list of variables to gather along with the traces
            \return STAT_OK on success
        */
        StatError_t sampleStackTraces(StatSample_t sampleType, bool withThreads, bool withPython, bool clearOnSample, unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, bool blocking = true, char *variableSpecification = "NULL");

        //! Collect the last stack trace from all daemons
        /*!
            \param blocking - [optional] whether to block until stack traces are received
            \return STAT_OK on success
        */
        StatError_t gatherLastTrace(bool blocking = true);

        //! Collect the accumulated stack traces from all daemons
        /*!
            \param blocking - [optional] whether to block until stack traces are received
            \return STAT_OK on success
        */
        StatError_t gatherTraces(bool blocking = true);

        //! Return the fileName of the last outputted dot file
        /*!
            \return the path of the last dot file generated
        */
        char *getLastDotFilename();

        //! Shutdown STAT
        /*!
            Shuts down the MRNet tree and closes the LaunchMON session
        */
        void shutDown();

        //! Detach from the application
        /*!
            \param blocking - [optional] set to true if blocks until all BE acks received
            \return STAT_OK on success

            Broadcast a message to the daemons to detach and await all acks
        */
        StatError_t detachApplication(bool blocking = true);

        //! Detach from the application
        /*!
            \param stopList - the list of MPI ranks to leave stoppped
            \param stopListSize - the number of elements in the stopList
            \param blocking - [optional] set to true if blocks until all BE acks received
            \return STAT_OK on success

            Broadcast a message to the daemons to detach and await all acks
        */
        StatError_t detachApplication(int *stopList, int stopListSize, bool blocking = true);

        //! Terminate the application
        /*!
            \param blocking - [optional] set to true if blocks until all BE acks received
            \return STAT_OK on success

            Broadcast a message to the daemons to terminate and await all acks
        */
        StatError_t terminateApplication(bool blocking = true);

        //! Print the STAT error type and error message
        /*!
            \param statError - the STAT error type
            \param sourceFile - the source file where the error was reported.  Set to NULL to omit printing of timestamp and source:line
            \param sourceLine - the source line where the error was reported.  Set to -1 to omit printing of timestamp and source:line
            \param fmt - the output format string
            \param ... - any variables to output
        */
        void printMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...);

        //! Create the log file and begin logging
        /*!
            \param logType - the level of logging
            \param logOutDir - the output directory for the log files
            \return STAT_OK on success
        */
        StatError_t startLog(unsigned char logType, char *logOutDir);

        //! Receive an acknowledgement from the daemons
        /*!
            \param blocking - [optional] whether or not to block on the acknowledgement
            \return STAT_OK on success
        */
        StatError_t receiveAck(bool blocking = true);

        //! Broadcast a message to to generate traces
        /*!
            \param maxDepth - the max depth of the generated traces
            \param nTasks - the number of tasks to emulate
            \param nTraces - the number of traces to generate per emulated task
            \param functionFanout - the maximum function fanout
            \param nEqClasses - the number of equivalence classes to generate
            \return STAT_OK on success
        */
        StatError_t statBenchCreateStackTraces(unsigned int maxDepth, unsigned int nTasks, unsigned int nTraces, unsigned int functionFanout, int nEqClasses, bool countRep);

        //! Collect the full incoming-edge label for the specified node
        /*!
            \param nodeId - the ID of the node
            \return the text node label
        */
        char *getNodeInEdge(int nodeId);

        //! Creates the ranks list
        /*!
            \return STAT_OK on success

            Parses the MRNet topology to determine the merging order and generates
            the correspondingly ordered ranks list.  This requires generating a tree
            based on the MRNet tree that maintains the lowest ranked MPI process
            beneath each node in the tree.  Alternatively, we could derrive this list
            by creating a new MRNet filter function.
        */
        StatError_t setRanksList();

        //! Set the flag to indicate a fatal error
        /*!
            \param hasFatalError - whether a fatal error has been detected

            Indicate to the STAT FrontEnd that a fatal error has occurred.
        */
        void setHasFatalError(bool hasFatalError);

        /**********************************************/
        /* Function to access and set class variables */
        /**********************************************/

        //! Print the list of communication  to stdout
        void printCommunicationNodeList();

        //! Print the list of application nodes to stdout
        void printApplicationNodeList();

        //! Gets the PID of the jobs launcher
        /*!
            \return the job launcher's PID
        */
        unsigned int getLauncherPid();

        //! gets the number of application processes
        /*!
            \return the number of application processes
        */
        unsigned int getNumApplProcs();

        //! Gets the number of application nodes
        /*!
            \return the number of application nodes
        */
        unsigned int getNumApplNodes();

        //! Sets the job ID
        /*!
            \param jobId - the job ID

            The specified job ID will be appended to the STAT run's output directory.
            This is helpful for example to associate a STAT run with a particular batch
            job ID.
        */
        void setJobId(unsigned int jobId);

        //! Gets the job ID
        /*!
            \return the job ID
        */
        unsigned int getJobId();

        //! Gets the name of the application executable
        /*!
            \return the application executable name
        */
        const char *getApplExe();

        //! Sets the path of the STAT daemon executable
        /*!
            \param toolDaemonExe - the path of the STAT daemon executable
            \return STAT_OK on success
        */
        StatError_t setToolDaemonExe(const char *toolDaemonExe);

        //! Gets the path of the STAT daemon executable
        /*!
            \return the path of the STAT daemon executable
        */
        const char *getToolDaemonExe();

        //! Sets the output directory
        /*!
            \param outDir - the output directory
            \return STAT_OK on success
        */
        StatError_t setOutDir(const char *outDir);

        //! Gets the output directory
        /*!
            \return the output directory
        */
        const char *getOutDir();

        //! Sets the output fileName prefix
        /*!
            \param filePrefix - the output fileName prefix
            \return STAT_OK on success
        */
        StatError_t setFilePrefix(const char *filePrefix);

        //! Gets the output fileName prefix
        /*!
            \return the output fileName prefix
        */
        const char *getFilePrefix();

        //! Sets the max number of communication processes to launch per node
        /*!
            \param procsPerNode - the max number of communication processes per node
        */
        void setProcsPerNode(unsigned int procsPerNode);

        //! Gets the max number of communication processes to launch per node
        /*!
            \return the max number of communication processes per node
        */
        unsigned int getProcsPerNode();

        //! Sets the STAT filter path
        /*!
            \param filterPath - the STAT filter path
            \return STAT_OK on success
        */
        StatError_t setFilterPath(const char *filterPath);

        //! Gets the STAT filter path
        /*!
            \return the STAT filter path
        */
        const char *getFilterPath();

        //! Gets the remote host to debug
        /*!
            \return the remote host to debug
        */
        const char *getRemoteNode();

        //! Adds an argument for the job launcher
        /*!
            \param launcherArg - the argument for the job launcher
            \return STAT_OK on success
        */
        StatError_t addLauncherArgv(const char *launcherArg);

        //! Gets the argument list for the job launcher
        /*!
            \return the arguement list
        */
        const char **getLauncherArgv();

        //! Gets the number of arguments for the job launcher
        /*!
            \return the number of arguments
        */
        unsigned int getLauncherArgc();

        //! Sets the verbosity output level
        /*!
            \param verbose - verbosity output level
        */
        void setVerbose(StatVerbose_t verbose);

        //! Gets the verbosity output level
        /*!
            \return the verbosity output level
        */
        StatVerbose_t getVerbose();

        //! Returns the last error message
        char *getLastErrorMessage();

        //! Adds a performance metric
        /*!
            \param buf - the string describing the metric
            \param time - the time in seconds
            \return STAT_OK on success
        */
        StatError_t addPerfData(const char *buf, double time);


        //! Sets the file to use when reading node list
        /*!
            \param nodeListFile - the File to read from
        */
        void setNodeListFile(char *nodeListFile);

        //! Gets the STAT install prefix
        /*!
            First use the STAT_PREFIX environment variable if it is defined,
            otherwise use the compile-time STAT_PREFIX macro.

            \return the installation prefix
        */
        const char *getInstallPrefix();

        //! Gets the STAT version number
        /*!
            \param[out] version - the version 3-tuple (caller must allocate int [3])
        */
        void getVersion(int *version);

    private:
        //! Perform operations required after attach acknowledgement
        /*!
            \return STAT_OK on success

            Update state and timing information
        */
        StatError_t postAttachApplication();

        //! Perform operations required after pause acknowledgement
        /*!
            \return STAT_OK on success

            Update state and timing information
        */
        StatError_t postPauseApplication();

        //! Perform operations required after resume acknowledgement
        /*!
            \return STAT_OK on success

            Update state and timing information
        */
        StatError_t postResumeApplication();

        //! Perform operations required after sample acknowledgement
        /*!
            \return STAT_OK on success

            Update state and timing information
        */
        StatError_t postSampleStackTraces();

        //! Perform operations required after detach acknowledgement
        /*!
            \return STAT_OK on success
        */
        StatError_t postDetachApplication();

        //! Perform operations required after terminate acknowledgement
        /*!
            \return STAT_OK on success
        */
        StatError_t postTerminateApplication();

        //! Shuts down the MRNet tree
        /*!
            \return STAT_OK on success
        */
        StatError_t shutdownMrnetTree();

        //! Dumps the process table to a file
        /*!
            \return STAT_OK on success
        */
        StatError_t dumpProctab();

        //! The actual gather implementation
        /*!
            \param type - the gather type tag, either PROT_SEND_LAST_TRACE or PROT_SEND_TRACES
            \param blocking - [optional] whether to block until stack traces are received
            \return STAT_OK on success
        */
        StatError_t gatherImpl(StatProt_t type, bool blocking = true);

        //! Collect the accumulated stack traces from all daemons
        /*!
            \param blocking - [optional] whether to block until stack traces are received
            \return STAT_OK on success

            First gathers the merged traces from the daemon.  Since the merge is an
            append operation, the bit vectors are out of order, requiring the bit vectors
            to be rearranged in MPI rank order.  Finally, the merged traces are
            outputted to a file.
        */
        StatError_t receiveStackTraces(bool blocking = true);

        //! Launch the STAT daemons
        /*!
            \param applicationOption - STAT_LAUNCH or STAT_ATTACH
            \param isStatBench - [optional] set to true if running as STATBench
            \return STAT_OK on success

            Performs LaunchMON setup, launches the daemons, and gathers the proc table.
            Also performs some initialization such as preparing the output directory,
            dumping the process table to a file, and generating the application node
            list for MRNet to topology file creation.
        */
        StatError_t launchDaemons(StatLaunch_t applicationOption, bool isStatBench = false);

        //! Send MRNet LeafInfo to the master daemon
        /*!
            \return STAT_OK on success

            Calls LaunchMON's send routine, which uses STAT's registered pack function to
            create a buffer out of the connection information.
        */
        StatError_t sendDaemonInfo();

        //! Create the MRNet topology file
        /*!
            \param topologyFileName - the path of the topology file to create
            \param topologyType - the topology specification type
            \param topologySpecification - the topology specification
            \param nodeList - the list of nodes for CPs
            \return STAT_OK on success

            Topologies can be based on max fanout, depth, or user defined.  User defined
            topologies specify the number of communication processes per layer of the
            tree separated by dashes, for example: 4, 4-16, 5-20-75
        */
        StatError_t createTopology(char *topologyFileName, StatTopology_t topologyType, char *topologySpecification, char *nodeList, bool shareAppNodes);

        //! Set the list of application nodes from the process table
        /*!
            \return STAT_OK on success

            Also prepares the ranks map required to reorder the bit vectors
        */
        StatError_t setDaemonNodes();

        //! Set the list of daemon nodes from the MRNet topology
        /*!
            \return STAT_OK on success

            Also prepares the ranks map required to reorder the bit vectors
        */
        StatError_t setAppNodeList();

        //! Set the ranks list for each daemon and place into the ranks map
        /*!
            \return STAT_OK on success

            This map of lists will be used to translate the bit vectors into MPI rank order
        */
        StatError_t createDaemonRankMap();

        //! Set the list of communication process nodes from the nodeList specification
        /*!
            \param nodeList - the list of nodes
            \return STAT_OK on success
        */
        StatError_t setCommNodeList(char *nodeList, bool checkAccess);

        //! Create the run specific output directory
        /*!
            \return STAT_OK on success
        */
        StatError_t createOutputDir();

        //! Check the version number with the BEs
        /*!
            \return STAT_OK on success
        */
        StatError_t checkVersion();

        //! Dumps the performance data to a file
        /*!
            \return STAT_OK on success
        */
        StatError_t dumpPerf();

        //! Set the list of application nodes from the process table
        /*!
            \return STAT_OK on success

            This only differs from setAppNodeList in that it assigns one BE per
            processor, not per node
        */
        StatError_t STATBench_setAppNodeList();

        //! Read the configuration file to get a list of potential nodes
        /*!
            \param[out] nodeList - the list of nodes from the configuration file
            \return STAT_OK on success
        */
        StatError_t setNodeListFromConfigFile(char **nodeList);

        //! Recursively build the rank tree
        /*!
            \param node - the corresponding MRNet topology node
            \param rankToNode - the map of ranks to node
            \return the node in the rank tree corresponding to the MRNet node

            Recursively builds the rank tree based on the MRNet topology.
        */
        //RemapNode_t *buildRemapTree(MRN::NetworkTopology::Node *node, std::map<int, RemapNode_t *> rankToNode);
        RemapNode_t *buildRemapTree(MRN::NetworkTopology::Node *node);

        //! Recursively generate the ranks list
        /*!
            \param node - the current node
            \return STAT_OK on success

            Depth first traversal, traversing the children from lowest rank to
            highest rank.
        */
        StatError_t buildRanksList(RemapNode_t *node);

        //! Recursively free up the allocated remap tree
        /*!
            \param node - the current node
            \return STAT_OK on success
        */
        StatError_t freeRemapTree(RemapNode_t *node);

        //! Check if the user has access to the specified node
        /*!
            \param node - the hostname to test access
            \return true if node is accessible
        */
        bool checkNodeAccess(char *node);

#ifdef STAT_FGFS
        StatError_t sendFileReqStream();
        StatError_t waitForFileRequests(unsigned int *streamId, int *returnTag, MRN::PacketPtr &packetPtr, int &retval);
#endif

        /****************/
        /* Private data */
        /****************/

        unsigned int launcherPid_;                          /*!< the PID of the job launcher */
        unsigned int nApplNodes_;                           /*!< the number of application nodes */
        unsigned int proctabSize_;                          /*!< the size of the process table */
        unsigned int nApplProcs_;                           /*!< the number of application processes */
        unsigned int procsPerNode_;                         /*!< the number of CPs to launcher per node*/
        unsigned int launcherArgc_;                         /*!< the number of job launch arguments*/
        unsigned int topologySize_;                         /*!< the size of the MRNet topology */
        int jobId_;                                         /*!< the batch job ID */
        int lmonSession_;                                   /*!< the LaunchMON session ID */
        int mrnetOutputLevel_;                              /*!< the MRNet output level */
        char **launcherArgv_;                               /*!< the job launch arguments */
        char *toolDaemonExe_;                               /*!< the path to the STATD daemon executable */
        char *applExe_;                                     /*!< the name of the application executable */
        char *filterPath_;                                  /*!< the path to the STAT_FilterDefinitions.so filter file */
        char *remoteNode_;                                  /*!< the hostname of the remote node running the job launcher */
        char *nodeListFile_;                                /*!< The file to containing the nodelist */
        char lastDotFileName_[BUFSIZE];                     /*!< the path to the last generated .dot file */
        char outDir_[BUFSIZE];                              /*!< the output directory */
        char logOutDir_[BUFSIZE];                           /*!< the directory for log files */
        char filePrefix_[BUFSIZE];                          /*!< the installation prefix for STAT */
        char lastErrorMessage_[BUFSIZE];                    /*!< the last error message */
        char hostname_[BUFSIZE];                            /*!< the FrontEnd hostname*/
        bool isLaunched_;                                   /*!< whether the STAT daemons have been launched */
        bool isConnected_;                                  /*!< whether the STAT daemons are connected to the MRNet tree */
        bool isAttached_;                                   /*!< whether the STAT daemons are attached to the application */
        bool isRunning_;                                    /*!< whether the application processes are currently running */
        bool isPendingAck_;                                 /*!< whether there are any pending acknowledgements */
        bool hasFatalError_;                                /*!< whether a fatal error has been detected outside of the STAT object */
        std::list<int> remapRanksList_;                     /*!< the order of bit vectors in the incoming packets */
        std::set<std::string> communicationNodeSet_;        /*!< the list of nodes to use for MRNet CPs */
        std::multiset<std::string> applicationNodeMultiSet_;    /*!< the set of application nodes */
        std::map<int, IntList_t *> mrnetRankToMPIRanksMap_; /*!< a map of MRNet ranks to ranks list used for bit vector reordering */
        std::set<int> missingRanks_;                        /*!< a set of MPI ranks whose daemon is not connected */
        std::vector<std::pair<std::string, double> > performanceData_;     /*!< the accumulated performance data to be dumped upon completion */
        LeafInfo_t leafInfo_;                               /*!< the MRNet leaf info */
        StatProt_t pendingAckTag_;                          /*!< the expected tag of the pending acknowledgement */
        StatError_t (STAT_FrontEnd::*pendingAckCb_)();      /*!< the function to call after acknowledgement received from daemons */
        StatVerbose_t verbose_;                             /*!< the verbosity level */
        unsigned char logging_;                             /*!< the logging level */
        MPIR_PROCDESC_EXT *proctab_;                        /*!< the process table */
        MRN::Network *network_;                             /*!< the MRNet Network object */
        MRN::Communicator *broadcastCommunicator_;          /*!< the broadcast communicator*/
        MRN::Stream *broadcastStream_;                      /*!< the broadcast stream for sending commands and receiving ack */
        MRN::Stream *mergeStream_;                          /*!< the merge stream that uses the STAT filter */
#ifdef STAT_FGFS
        char *fgfsFilterPath_;
        MRN::Stream *perfStream_;
        MRN::Stream *fileRequestStream_;
        MRN::Stream *fgfsStream_;
        FastGlobalFileStatus::CommLayer::CommFabric *fgfsCommFabric_;
#endif
};

#endif /* #define __STAT_FRONTEND_H */
