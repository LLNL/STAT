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

#ifdef STAT_FGFS
#include <stdint.h> 
#include <cstring>
#endif

#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include "config.h"
#include "mrnet/Packet.h"
#include "graphlib.h"
#include "STAT.h"
#include <sys/stat.h>

using namespace MRN;
using namespace std;

extern "C" {

//! The MRNet format string for the STAT filter
const char *statMerge_format_string = "%ac %d %d";

//! The MRNet format string for the STAT version check
const char *STAT_checkVersion_format_string = "%d %d %d %d %d";

#if (defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT))
//! Increases nofile and nproc limits to maximum
/*!
    \return STAT_OK on success

    Increases limits to get a full core file on error for debugging.
*/
StatError_t increaseCoreLimit()
{
    struct rlimit rlim[1];
    StatError_t statError = STAT_OK;

    if (getrlimit(RLIMIT_CORE, rlim) < 0)
    {
        perror("getrlimit failed:\n");
        statError = STAT_SYSTEM_ERROR;
    }
    else if (rlim->rlim_cur < rlim->rlim_max)
    {
        rlim->rlim_cur = rlim->rlim_max;
        if (setrlimit (RLIMIT_CORE, rlim) < 0)
        {
            perror("Unable to increase max no. files:");
            statError = STAT_SYSTEM_ERROR;
        }
    }
    return statError;
}
#endif

char lastErrorMessage_[BUFSIZE];
unsigned char logging_ = STAT_LOG_NONE;
void cpPrintMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...)
{
    va_list arg;
    char timeString[BUFSIZE];
    const char *timeFormat = "%b %d %T";
    time_t currentTime;
    struct tm *localTime;

    /* If this is a log message and we're not logging, then return */
    if (statError == STAT_LOG_MESSAGE && statOutFp == NULL)
        return;

    /* Get the time */
    currentTime = time(NULL);
    localTime = localtime(&currentTime);
    if (localTime == NULL)
        snprintf(timeString, BUFSIZE, "NULL");
    else
        strftime(timeString, BUFSIZE, timeFormat, localTime);

    if (statError != STAT_LOG_MESSAGE && statError != STAT_STDOUT && statError != STAT_VERBOSITY)
    {
        /* Print the error to the screen (or designated error file) */
        fprintf(stderr, "<%s> <%s:% 4d> ", timeString, sourceFile, sourceLine);
        fprintf(stderr, "STAT returned error type ");
        statPrintErrorType(statError, stderr);
        fprintf(stderr, ": ");
        va_start(arg, fmt);
        vsnprintf(lastErrorMessage_, BUFSIZE, fmt, arg);
        va_end(arg);
        fprintf(stderr, "%s", lastErrorMessage_);
    }
   
    /* Print the message to the log */
    if (statOutFp != NULL)
    {
        if (logging_ & STAT_LOG_MRN)
        {
            char msg[BUFSIZE];
            va_start(arg, fmt);
            vsnprintf(msg, BUFSIZE, fmt, arg);
            va_end(arg);
            mrn_printf(sourceFile, sourceLine, "", statOutFp, "%s", msg);
        }
        else
        {
            if (sourceLine != -1 && sourceFile != NULL)
                fprintf(statOutFp, "<%s> <%s:%d> ", timeString, sourceFile, sourceLine);
            if (statError != STAT_LOG_MESSAGE && statError != STAT_STDOUT && statError != STAT_VERBOSITY)
            {
                fprintf(statOutFp, "STAT returned error type ");
                statPrintErrorType(statError, statOutFp);
                fprintf(statOutFp, ": ");
            }    
            va_start(arg, fmt);
            vfprintf(statOutFp, fmt, arg);
            va_end(arg);
            fflush(statOutFp);
        }
    }
}

const char *filterInit_format_string = "%uc %s %d";
void filterInit(vector<PacketPtr> &packetsIn,
                           vector<PacketPtr> &packetsOut,
                           vector<PacketPtr> &packetsOutReverse,
                           void **filterState,
                           PacketPtr &params,
                           const TopologyLocalInfo &topology)
{
    char *logDir;
    char fileName[BUFSIZE], hostname[BUFSIZE];
    int ret, mrnetOutputLevel;
    unsigned int i;

    if (packetsIn[0]->get_Tag() == PROT_SEND_BROADCAST_STREAM)
    {
        for (i = 0; i < packetsIn.size(); i++)
        {
            if (packetsIn[i]->unpack("%uc %s %d", &logging_, &logDir, &mrnetOutputLevel) == -1)
                cpPrintMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "failed to unpack packet\n");
            if (topology.get_Network()->is_LocalNodeInternal())
            {
                if (logging_ & STAT_LOG_CP)  
                {
                    /* Create the log directory */
                    ret = mkdir(logDir, S_IRUSR | S_IWUSR | S_IXUSR); 
                    if (ret == -1 && errno != EEXIST)
                    {
                        cpPrintMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: mkdir failed to create log directory %s\n", strerror(errno), logDir);
                    }
                    ret = gethostname(hostname, BUFSIZE);
                    if (ret != 0)
                        cpPrintMsg(STAT_WARNING, __FILE__, __LINE__, "Warning, Failed to get hostname\n");
                    snprintf(fileName, BUFSIZE, "%s/%s.STATfilter.%d.log", logDir, hostname, topology.get_Rank());
                    statOutFp = fopen(fileName, "w");
                    if (statOutFp == NULL)
                        cpPrintMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed to open FE log file %s\n", strerror(errno), fileName);
                    if (logging_ & STAT_LOG_MRN)
                    {
                        mrn_printf_init(statOutFp);
                    }
                    set_OutputLevel(mrnetOutputLevel);
                }
            }
        }
    }
    for (i = 0; i < packetsIn.size(); i++)
        packetsOut.push_back(packetsIn[i]);
}

//! A message to check the version of various components
/*!
    \param inputPackets - the vector of input packets
    \param outputPackets - the vector of output packets
*/
void STAT_checkVersion(vector<PacketPtr> &inputPackets,
                       vector<PacketPtr> &outputPackets,
                       void **)
{
    PacketPtr currentPacket;
    int major, minor, revision, i;
    int daemonCount = 0, filterCount = 0;

    /* See if we're checking the filter version */
    if (inputPackets[0]->get_Tag() == PROT_CHECK_VERSION_RESP)
    {
        for (i = 0; i < inputPackets.size(); i++)
        {
            /* Update the filter and daemon counts */
            currentPacket = inputPackets[i];
            major = (*currentPacket)[0]->get_int32_t();
            minor = (*currentPacket)[1]->get_int32_t();
            revision = (*currentPacket)[2]->get_int32_t();
            daemonCount += (*currentPacket)[3]->get_int32_t();
            filterCount += (*currentPacket)[4]->get_int32_t();
        }

        /* Prepare the output values */
        if (major != STAT_MAJOR_VERSION || minor != STAT_MINOR_VERSION || revision != STAT_REVISION_VERSION)
        {
            cpPrintMsg(STAT_VERSION_ERROR, __FILE__, __LINE__, "Filter reports version mismatch: FE = %d.%d.%d, Filter = %d.%d.%d\n", major, minor, revision, STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            filterCount += 1;
        }

        /* Send the packet */
        PacketPtr newPacket(new Packet(inputPackets[0]->get_StreamId(), inputPackets[0]->get_Tag(), "%d %d %d %d %d", major, minor, revision, daemonCount, filterCount));
        outputPackets.push_back(newPacket);
        return;
    }
}

//! Merges graphs from incoming packets into a single out packet
/*!
    \param inputPackets - the vector of input packets
    \param outputPackets - the vector of output packets
*/
void statMerge(vector<PacketPtr> &inputPackets,
                vector<PacketPtr> &outputPackets,
                void ** /* client data */)
{
    static int totalWidth;
    int nChildren, *edgeLabelWidths, rank, inputRank, outputRank;
    unsigned int outputByteArrayLen;
    graphlib_graph_p returnGraph = NULL, currentGraph = NULL;
    graphlib_error_t gl_err;
    map<int,int> childrenOrder;
    map<int,int>::iterator iter;
    /* The byte arrays are declared static so we can manage the allocated memory
       This is because we need to be assured that the data exists when MRNet 
       tries to send it.  They are safe to free up on the next invocation of 
       this filter function */
    static char *outputByteArray;
    DataType type;
    unsigned int byteArrayLen, i, j;
    char *byteArray;
    int child;
    PacketPtr currentPacket;

#if (defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT))
//    static int init = 0;
//    if (init == 0)
//        increaseCoreLimit();
//    init++;
#endif

    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "STAT filter invoked\n");

    /* Delete byte arrays from previous iterations */
    if (outputByteArray != NULL)
        free(outputByteArray);

    /* TODO: if there's only one packet, just ship it back out! */
    /* We want to make sure we're processing packets in the same order 
       every time the filter is invoked.  That way, we know where in the 
       final bit vector to place each incoming bit vector */
    for (i = 0; i < inputPackets.size(); i++)
    {
        currentPacket = inputPackets[i];
        rank = (*currentPacket)[2]->get_int32_t();
        childrenOrder[rank] = i;
    }

    /* Initialize graphlib */
    nChildren = inputPackets.size();
    edgeLabelWidths = (int *)malloc(nChildren * sizeof(int));
    if (edgeLabelWidths == NULL)
    {
        cpPrintMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to allocate edgeLabelWidths\n", strerror(errno));
        return;
    }
    i = 0;
    for (iter = childrenOrder.begin(); iter != childrenOrder.end(); iter++)
    {
        currentPacket = inputPackets[iter->second];
        edgeLabelWidths[i] = (*currentPacket)[1]->get_int32_t();
        i++;
    }
    gl_err = graphlib_InitVarEdgeLabelsConn(nChildren, edgeLabelWidths, &totalWidth);
    free(edgeLabelWidths);
    if (GRL_IS_FATALERROR(gl_err))
    {
        cpPrintMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to initialize graphlib\n");
        return;
    }

    /* Initialize the result graphs */
    gl_err = graphlib_newGraph(&returnGraph);
    if (GRL_IS_FATALERROR(gl_err))
    {
        cpPrintMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__,"Failed to create new graph\n");
        return;
    }

    /* Loop around the packets in order of lowest task rank and merge into output */
    rank = -1;
    for (iter = childrenOrder.begin(); iter != childrenOrder.end(); iter++)
    {
        rank++;
        child = iter->second;
        currentPacket = inputPackets[child];

        /* Deserialize 1st graph in packet element [0] */
        byteArray = (char *)((*currentPacket)[0]->get_array(&type, &byteArrayLen));
        gl_err = graphlib_deserializeGraphConn(rank, &currentGraph, byteArray, byteArrayLen);
        if (GRL_IS_FATALERROR(gl_err))
        {
            cpPrintMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to deserialize graph %d\n", rank);
            return;
        }

        /* Merge graph 1 into 1st returnGraph */
        gl_err = graphlib_mergeGraphsRanked(returnGraph, currentGraph);
        if (GRL_IS_FATALERROR(gl_err))
        {
           cpPrintMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to merge graph %d\n", rank);
            return;
        }

        /* Delete the current graph since we no longer need it */
        gl_err = graphlib_delGraph(currentGraph);
        if (GRL_IS_FATALERROR(gl_err))
        {
            cpPrintMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to delete graph %d\n", rank);
            return;
        }

        /* Add the array of ranks associated with this packet */
        inputRank = (*currentPacket)[2]->get_int32_t();
        if (rank == 0)
            outputRank = inputRank;
        else if (inputRank < outputRank)
            outputRank = inputRank;
    }

    /* Now to finish up: serialize both result graphs to create output packet */
    gl_err = graphlib_serializeGraph(returnGraph, &outputByteArray, &outputByteArrayLen);
    if (GRL_IS_FATALERROR(gl_err))
    {
        cpPrintMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to serialize output graph\n");
        return;
    }
    PacketPtr newPacket(new Packet(inputPackets[0]->get_StreamId(), inputPackets[0]->get_Tag(), "%ac %d %d", outputByteArray, outputByteArrayLen, totalWidth, outputRank));
    outputPackets.push_back(newPacket);

    /* Delete the result graphs since we no longer need them */
    gl_err = graphlib_delGraph(returnGraph);
    if (GRL_IS_FATALERROR(gl_err))
    {
        cpPrintMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to delete output graph\n");
        return;
    }
}

#ifdef STAT_FGFS

/* New Filters for solving the Scalabilty problem of accesing files */
map<string, pair<char*, int> > fileNameToContentsMap;
set<string> visitedFileSet;
pthread_mutex_t fileNameToContentsMapMutex, visitedFileSetMutex;

const char *fileRequestUpStream_format_string = "%s";
void fileRequestUpStream(vector<PacketPtr> &packetsIn,
                         vector<PacketPtr> &packetsOut,
                         vector<PacketPtr> &packetsOutReverse,
                         void **filterState,
                         PacketPtr &params,
                         const TopologyLocalInfo &topology)
{
    char *fileName;
    char *fileContents;
    unsigned int i;
    PacketPtr currentPacket, newPacket;
    map<string, pair<char *, int> >::iterator iter;

    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "File request up stream filter begin.\n");

    if (packetsIn[0]->get_Tag() == PROT_FILE_REQ_RESP)
    {
        newPacket = packetsIn[0];
        packetsOut.push_back(newPacket);
    }
    else
    {
        for (i = 0; i < packetsIn.size(); i++)
        {
            currentPacket = packetsIn[i];
            currentPacket->unpack("%s", &fileName);
            pthread_mutex_lock(&fileNameToContentsMapMutex);
            iter = fileNameToContentsMap.find(fileName);
            if (iter != fileNameToContentsMap.end())
            {
                fileContents = (char *)malloc((iter->second.second) * sizeof(char));
                memcpy(fileContents, iter->second.first, iter->second.second);
                pthread_mutex_unlock(&fileNameToContentsMapMutex);
                cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending cached result for %s in reverse.\n", fileName);
                PacketPtr newPacketReverse(new Packet(packetsIn[0]->get_StreamId(),
                                           packetsIn[0]->get_Tag(), "%auc %s", fileContents,
                                           iter->second.second, fileName));
                packetsOutReverse.push_back(newPacketReverse);
            }
            else
            {
                pthread_mutex_unlock(&fileNameToContentsMapMutex);
                pthread_mutex_lock(&visitedFileSetMutex);
                if (visitedFileSet.find(fileName) == visitedFileSet.end())
                {
                    visitedFileSet.insert(fileName);
                    pthread_mutex_unlock(&visitedFileSetMutex);
                    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Noting request for %s.\n", fileName);
                    packetsOut.push_back(packetsIn[i]);
                }
                else
                {
                    pthread_mutex_unlock(&visitedFileSetMutex);
                    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "%d: Duplicate request for %s.\n", topology.get_Network()->get_LocalRank(), fileName);
                }
            }
        }
    }
    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "File request up stream filter end.\n");
}


const char *fileRequestDownStream_format_string = "%ac %s";
void fileRequestDownStream(vector<PacketPtr> &packetsIn,
                           vector<PacketPtr> &packetsOut,
                           vector<PacketPtr> &packetsOutReverse,
                           void **filterState,
                           PacketPtr &params,
                           const TopologyLocalInfo &topology)
{
    char *fileContents;
    char *fileName;
    int fileContentsLength;
    unsigned int i;
    PacketPtr currentPacket, newPacket;
    set<string>::iterator iter;
    map<string,pair<char*,int> >::iterator iter2;
    static int totalLength = 0;

    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "File request down stream filter begin.\n");
    if (packetsIn[0]->get_Tag() == PROT_FILE_REQ)
    {
        cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "PROT_FILE_REQ\n"); 
//        set_OutputLevel(2); //TODO: this should not be used for STAT logging... designate tag for log enabling?
//        if (statOutFp == NULL)
//        {
//            MRN_DEBUG_LOG_DIRECTORY = "/g/g0/lee218/logs";
//            statOutFp = stderr;
//        }
        pthread_mutex_init(&fileNameToContentsMapMutex, NULL);
        pthread_mutex_init(&visitedFileSetMutex, NULL);
        newPacket = packetsIn[0];
        packetsOut.push_back(newPacket);
    }
    else
    {
        cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "!PROT_FILE_REQ\n"); 

        for (i = 0; i < packetsIn.size(); i++)
        {
            currentPacket = packetsIn[i];
            currentPacket->unpack("%ac %s", &fileContents, &fileContentsLength, &fileName);
            if (topology.get_Network()->is_LocalNodeInternal())
            { 
                totalLength += fileContentsLength;                        
                cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "caching contents of %s at CP length %d total %d\n", fileName, fileContentsLength, totalLength); 
                pthread_mutex_lock(&fileNameToContentsMapMutex);
                iter2 = fileNameToContentsMap.find(fileName);
                if (iter2 == fileNameToContentsMap.end())
                    fileNameToContentsMap[fileName] = make_pair(fileContents, fileContentsLength);
                pthread_mutex_unlock(&fileNameToContentsMapMutex);
            }

            pthread_mutex_lock(&visitedFileSetMutex);
            iter = visitedFileSet.find(fileName);
            if (iter != visitedFileSet.end())
            {        
                visitedFileSet.erase(iter);
                pthread_mutex_unlock(&visitedFileSetMutex);
                cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "passing along contents of %s at CP\n", fileName); 
                packetsOut.push_back(packetsIn[i]);
            }
            else
                pthread_mutex_unlock(&visitedFileSetMutex);
        }
    }
    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "File request down stream filter end.\n");
}

#endif 

} /* extern "C" */
