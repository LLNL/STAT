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

#include "mrnet/Packet.h"
#include "graphlib.h"
#include "STAT.h"
#include <sys/resource.h>

using namespace MRN;
using namespace std;

extern "C" {

//! The MRNet format string for the STAT filter
const char *STAT_Merge_format_string = "%ac %d %d";

//! The MRNet format string for the STAT version check
const char *STAT_checkVersion_format_string = "%d %d %d %d %d";

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

//! A message to check the version of various components
/*!
    \param inputPackets - the vector of input packets
    \param outputPackets - the vector of output packets
*/
void STAT_checkVersion(std::vector < PacketPtr >&inputPackets,
                std::vector < PacketPtr >&outputPackets,
                void ** /* client data */)
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
            fprintf(stderr, "Filter reports version mismatch: FE = %d.%d.%d, Filter = %d.%d.%d\n", major, minor, revision, STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            filterCount += 1;
        }

        /* Send the packet */
        PacketPtr new_packet(new Packet(inputPackets[0]->get_StreamId(), inputPackets[0]->get_Tag(), "%d %d %d %d %d", major, minor, revision, daemonCount, filterCount));
        outputPackets.push_back(new_packet);
        return;
    }
}

//! Merges graphs from incoming packets into a single out packet
/*!
    \param inputPackets - the vector of input packets
    \param outputPackets - the vector of output packets
*/
void STAT_Merge(std::vector < PacketPtr >&inputPackets,
                std::vector < PacketPtr >&outputPackets,
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
    static int init = 0;
    DataType type;
    unsigned int byteArrayLen, i, j;
    char *byteArray;
    int child;

//    if (init == 0)
//        increaseCoreLimit();
//    init++;

    PacketPtr currentPacket;

    /* Delete byte arrays from previous iterations */
    if (outputByteArray != NULL)
        free(outputByteArray);

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
    i = -1;
    for (iter = childrenOrder.begin(); iter != childrenOrder.end(); iter++)
    {
        i++;
        currentPacket = inputPackets[iter->second];
        edgeLabelWidths[i] = (*currentPacket)[1]->get_int32_t();
    }
    gl_err = graphlib_InitVarEdgeLabelsConn(nChildren, edgeLabelWidths, &totalWidth);
    free(edgeLabelWidths);
    assert(!GRL_IS_FATALERROR(gl_err));

    /* Initialize the result graphs */
    gl_err = graphlib_newGraph(&returnGraph);
    assert(!GRL_IS_FATALERROR(gl_err));

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
        assert(!GRL_IS_FATALERROR(gl_err));

        /* Merge graph 1 into 1st returnGraph */
        gl_err = graphlib_mergeGraphsRanked(returnGraph, currentGraph);
        assert(!GRL_IS_FATALERROR(gl_err));

        /* Delete the current graph since we no longer need it */
        gl_err = graphlib_delGraph(currentGraph);
        assert(!GRL_IS_FATALERROR(gl_err));

        /* Add the array of ranks associated with this packet */
        inputRank = (*currentPacket)[2]->get_int32_t();
        if (rank == 0)
            outputRank = inputRank;
        else if (inputRank < outputRank)
            outputRank = inputRank;
    }

    /* Now to finish up: serialize both result graphs to create output packet */
    gl_err = graphlib_serializeGraph(returnGraph, &outputByteArray, &outputByteArrayLen);
    assert(!GRL_IS_FATALERROR(gl_err));
    PacketPtr new_packet(new Packet(inputPackets[0]->get_StreamId(), inputPackets[0]->get_Tag(), "%ac %d %d", outputByteArray, outputByteArrayLen, totalWidth, outputRank));
    outputPackets.push_back(new_packet);

    /* Delete the result graphs since we no longer need them */
    gl_err = graphlib_delGraph(returnGraph);
    assert(!GRL_IS_FATALERROR(gl_err));
}

} /* extern "C" */
