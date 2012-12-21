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

#include "STAT_GraphRoutines.h"

graphlib_functiontable_p statBitVectorFunctions;
graphlib_functiontable_p statMergeFunctions;
graphlib_functiontable_p statReorderFunctions;
graphlib_functiontable_p statCountRepFunctions;
int statGraphRoutinesTotalWidth;
int *statGraphRoutinesEdgeLabelWidths;
int statGraphRoutinesCurrentIndex;
int *statGraphRoutinesRanksList;
int statGraphRoutinesRanksListLength;

unsigned int statStringHash(const char *str)
{
    unsigned int hash = 0;
    int c;

    while (c = *(str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

graphlib_graph_p createRootedGraph(graphlib_functiontable_p functions)
{
    graphlib_error_t graphlibError;
    graphlib_graph_p newGraph = NULL;

    graphlibError = graphlib_newGraph(&newGraph, functions);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error creating new graph\n");
        return NULL;
    }

    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0,(char *) "/", GRAPH_FONT_SIZE};
    graphlibError = graphlib_addNode(newGraph, 0, &nodeAttr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error adding sentinel node to graph\n");
        return NULL;
    }
    return newGraph;
}

StatBitVectorEdge_t *initializeBitVectorEdge(int numTasks)
{
    StatBitVectorEdge_t *edge = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
    if (edge == NULL)
    {
        fprintf(stderr, "%s: Failed to malloc edge\n", strerror(errno));
        return NULL;
    }
    edge->length = statBitVectorLength(numTasks);
    edge->bitVector = (StatBitVector_t *)calloc(edge->length, STAT_BITVECTOR_BYTES);
    if (edge->bitVector == NULL)
    {
        fprintf(stderr, "%s: Failed to calloc %d longs for edge->bitVector\n", strerror(errno), edge->length);
        free(edge);
        return NULL;
    }
    return edge;
}

void statInitializeBitVectorFunctions()
{
    statFreeBitVectorFunctions();
    statBitVectorFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (statBitVectorFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    statBitVectorFunctions->serialize_node = statSerializeNode;
    statBitVectorFunctions->serialize_node_length = statSerializeNodeLength;
    statBitVectorFunctions->deserialize_node = statDeserializeNode;
    statBitVectorFunctions->node_to_text = statNodeToText;
    statBitVectorFunctions->merge_node = statMergeNode;
    statBitVectorFunctions->copy_node = statCopyNode;
    statBitVectorFunctions->free_node = statFreeNode;
    statBitVectorFunctions->serialize_edge = statSerializeEdge;
    statBitVectorFunctions->serialize_edge_length = statSerializeEdgeLength;
    statBitVectorFunctions->deserialize_edge = statDeserializeEdge;
    statBitVectorFunctions->edge_to_text = statEdgeToText;
    statBitVectorFunctions->merge_edge = statMergeEdge;
    statBitVectorFunctions->copy_edge = statCopyEdge;
    statBitVectorFunctions->free_edge = statFreeEdge;
    statBitVectorFunctions->edge_checksum = statEdgeCheckSum;
}

void statFreeBitVectorFunctions()
{
    if (statBitVectorFunctions != NULL)
        free(statBitVectorFunctions);
    statBitVectorFunctions = NULL;
}

void statInitializeMergeFunctions()
{
    statFreeMergeFunctions();
    statMergeFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (statMergeFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    statMergeFunctions->serialize_node = statSerializeNode;
    statMergeFunctions->serialize_node_length = statSerializeNodeLength;
    statMergeFunctions->deserialize_node = statDeserializeNode;
    statMergeFunctions->node_to_text = statNodeToText;
    statMergeFunctions->merge_node = statMergeNode;
    statMergeFunctions->copy_node = statCopyNode;
    statMergeFunctions->free_node = statFreeNode;
    statMergeFunctions->serialize_edge = statSerializeEdge;
    statMergeFunctions->serialize_edge_length = statSerializeEdgeLength;
    statMergeFunctions->deserialize_edge = statFilterDeserializeEdge;
    statMergeFunctions->edge_to_text = statEdgeToText;
    statMergeFunctions->merge_edge = statMergeEdge;
    statMergeFunctions->copy_edge = statCopyEdge;
    statMergeFunctions->free_edge = statFreeEdge;
    statMergeFunctions->edge_checksum = statEdgeCheckSum;
}

void statFreeMergeFunctions()
{
    if (statMergeFunctions != NULL)
        free(statMergeFunctions);
    statMergeFunctions = NULL;
}

void statInitializeReorderFunctions()
{
    statFreeReorderFunctions();
    statReorderFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (statReorderFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    statReorderFunctions->serialize_node = statSerializeNode;
    statReorderFunctions->serialize_node_length = statSerializeNodeLength;
    statReorderFunctions->deserialize_node = statDeserializeNode;
    statReorderFunctions->node_to_text = statNodeToText;
    statReorderFunctions->merge_node = statMergeNode;
    statReorderFunctions->copy_node = statCopyNode;
    statReorderFunctions->free_node = statFreeNode;
    statReorderFunctions->serialize_edge = statSerializeEdge;
    statReorderFunctions->serialize_edge_length = statSerializeEdgeLength;
    statReorderFunctions->deserialize_edge = statDeserializeEdge;
    statReorderFunctions->edge_to_text = statEdgeToText;
    statReorderFunctions->merge_edge = statMergeEdgeOrdered;
    statReorderFunctions->copy_edge = statCopyEdgeInitializeEmpty;
    statReorderFunctions->free_edge = statFreeEdge;
    statReorderFunctions->edge_checksum = statEdgeCheckSum;
}

void statFreeReorderFunctions()
{
    if (statReorderFunctions != NULL)
        free(statReorderFunctions);
    statReorderFunctions = NULL;
}

void statInitializeCountRepFunctions()
{
    statFreeCountRepFunctions();
    statCountRepFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (statCountRepFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    statCountRepFunctions->serialize_node = statSerializeNode;
    statCountRepFunctions->serialize_node_length = statSerializeNodeLength;
    statCountRepFunctions->deserialize_node = statDeserializeNode;
    statCountRepFunctions->node_to_text = statNodeToText;
    statCountRepFunctions->merge_node = statMergeNode;
    statCountRepFunctions->copy_node = statCopyNode;
    statCountRepFunctions->free_node = statFreeNode;
    statCountRepFunctions->serialize_edge = statSerializeCountRepEdge;///
    statCountRepFunctions->serialize_edge_length = statSerializeCountRepEdgeLength;///
    statCountRepFunctions->deserialize_edge = statDeserializeCountRepEdge;///
    statCountRepFunctions->edge_to_text = statCountRepEdgeToText;///
    statCountRepFunctions->merge_edge = statMergeCountRepEdge;///
    statCountRepFunctions->copy_edge = statCopyCountRepEdge;///
    statCountRepFunctions->free_edge = statFreeCountRepEdge;///
    statCountRepFunctions->edge_checksum = statCountRepEdgeCheckSum;///
}

void statFreeCountRepFunctions()
{
    if (statCountRepFunctions != NULL)
        free(statCountRepFunctions);
    statCountRepFunctions = NULL;
}

size_t statBitVectorLength(int numTasks)
{
    int ret;

    ret = numTasks / STAT_BITVECTOR_BITS;
    if (numTasks % STAT_BITVECTOR_BITS != 0)
        ret += 1;
    return ret;
}

void statSerializeNode(char *buf, const void *node)
{
  if (node!= NULL)
    strcpy(buf, (char *)node);
}

unsigned int statSerializeNodeLength(const void *node)
{
  if (node!=NULL)
    return strlen((char *)node) + 1;
  else
    return 0;
}

void statDeserializeNode(void **node, const char *buf, unsigned int bufLength)
{
  *node = malloc(bufLength);
  strcpy((char *)*node, buf);
}

char *statNodeToText(const void *node)
{
    return strdup((char *)node);
}

void *statMergeNode(void *node1, const void *node2)
{
    return node1;
}

void *statCopyNode(const void *node)
{
    return (void *)strdup((char *)node);
}

void statFreeNode(void *node)
{
    free(node);
}

void statSerializeEdge(char *buf, const void *edge)
{
    char *ptr = buf;
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;

    memcpy(ptr, (void *)&(e->length), sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(ptr, e->bitVector, STAT_BITVECTOR_BYTES * e->length);
}

unsigned int statSerializeEdgeLength(const void *edge)
{
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;
    return sizeof(size_t) + STAT_BITVECTOR_BYTES * e->length;
}

void statDeserializeEdge(void **edge, const char *buf, unsigned int bufLength)
{
    char *ptr = (char *)buf;
    StatBitVectorEdge_t *e;

    e = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
    if (e == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for deserialized edge object\n", sizeof(StatBitVectorEdge_t));
        return;
    }

    memcpy((void *)&(e->length), ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    e->bitVector = (StatBitVector_t *)malloc(STAT_BITVECTOR_BYTES * e->length);
    if (e == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for deserialized edge bit vector\n", STAT_BITVECTOR_BYTES * e->length);
        return;
    }
    memcpy(e->bitVector, ptr, STAT_BITVECTOR_BYTES * e->length);
    *edge = (void *)e;
}

char *statEdgeToText(const void *edge)
{
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;
    char val[128];
    int i, j;
    int in_range = 0, first_iteration = 1, range_start, range_end, cur_val, last_val = 0;
    char *ret;
    unsigned int ret_size = 0, count = 0;

    ret = (char *)malloc(STAT_GRAPH_CHUNK * sizeof(char));
    ret_size = STAT_GRAPH_CHUNK;
    if (ret == NULL)
    {
        fprintf(stderr, "%s: Failed to allocte memory for edge label\n", strerror(errno));
        return NULL;
    }
    sprintf(ret + count, "[");
    count += 1;
    for (i = 0; i< e->length; i++)
    {
        if (ret_size - count < 1024)
        {
            /* Reallocate if we are within 1024 bytes of the end */
            /* This is a large threshold to keep it out of the inner loop */
            ret_size += STAT_GRAPH_CHUNK;
            ret = (char *)realloc(ret, ret_size * sizeof(char));
            if (ret == NULL)
            {
                fprintf(stderr, "%s: Failed to reallocte %d bytes of memory for edge label\n", strerror(errno), ret_size);
                return NULL;
            }
        }
        for (j = 0; j < 8 * sizeof(StatBitVector_t); j++)
        {
            if (e->bitVector[i] & STAT_GRAPH_BIT(j))
            {
                cur_val = i * 8 * sizeof(StatBitVector_t) + j;
                if (in_range == 0)
                {
                    snprintf(val, 128, "%d", cur_val);
                    if (first_iteration == 0)
                    {
                        if (cur_val == last_val + 1)
                        {
                            in_range = 1;
                            range_start = last_val;
                            range_end = cur_val;
                            sprintf(ret + count, "-");
                            count += 1;
                        }
                        else
                        {
                            sprintf(ret + count, ",");
                            count += 1;
                            sprintf(ret + count, "%s", val);
                            count += strlen(val);
                        }
                    }
                    else
                    {
                        sprintf(ret + count, "%s", val);
                        count += strlen(val);
                    }
                }
                else
                {
                    if (cur_val == last_val + 1)
                        range_end = cur_val;
                    else
                    {
                        snprintf(val, 128, "%d,%d", last_val, cur_val);
                        sprintf(ret + count, "%s", val);
                        count += strlen(val);
                        in_range = 0;
                    }
                }
                first_iteration = 0;
                last_val = cur_val;
            }
        }
    }
    if (in_range == 1)
    {
        snprintf(val, 128, "%d", last_val);
        sprintf(ret + count, "%s", val);
        count += strlen(val);
    }
    sprintf(ret + count, "]");
    count += 1;

    return ret;
}

void *statMergeEdge(void *edge1, const void *edge2)
{
    unsigned int i;
    size_t length;
    StatBitVectorEdge_t *e1 = (StatBitVectorEdge_t *)edge1, *e2 = (StatBitVectorEdge_t *)edge2;
    length = e1->length;
    if (e2->length < e1->length)
        length = e2->length;
    for (i = 0; i < e1->length; i++)
        e1->bitVector[i] |= e2->bitVector[i];
    return edge1;
}

void *statCopyEdge(const void *edge)
{
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge, *ret;

    ret = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
    if (ret == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for edge copy\n", sizeof(StatBitVectorEdge_t));
        return NULL;
    }
    ret->length = e->length;
    ret->bitVector = (StatBitVector_t *)malloc(e->length * STAT_BITVECTOR_BYTES);
    if (ret->bitVector == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for bit vector\n", e->length * STAT_BITVECTOR_BYTES);
        return NULL;
    }
    memcpy(ret->bitVector, e->bitVector, STAT_BITVECTOR_BYTES * e->length);
    return (void *)ret;
}

void statFreeEdge(void *edge)
{
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;

    if (e->bitVector != NULL)
        free(e->bitVector);
    free(e);
}

long statEdgeCheckSum(const void *edge)
{
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;
    int i;
    long ret = 0;

    for (i = 0; i < e->length; i++)
        ret = ret + e->bitVector[i] * (e->length - i + 1);

    return ret;
}

void statFilterDeserializeEdge(void **edge, const char *buf, unsigned int bufLength)
{
    int offset, i;
    char *ptr = (char *)buf;
    size_t currentEdgeLength;
    StatBitVectorEdge_t *e;

    e = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
    if (e == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for deserialized edge object\n", sizeof(StatBitVectorEdge_t));
        return;
    }

    memcpy((void *)&(currentEdgeLength), ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    e->length = statGraphRoutinesTotalWidth;
    e->bitVector = (StatBitVector_t *)calloc(e->length, STAT_BITVECTOR_BYTES);
    if (e == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for deserialized edge bit vector\n", STAT_BITVECTOR_BYTES * e->length);
        return;
    }

    offset = 0;
    for (i = 0; i < statGraphRoutinesCurrentIndex; i++)
        offset += statGraphRoutinesEdgeLabelWidths[i];

    memcpy((void *)&(e->bitVector[offset]), ptr, STAT_BITVECTOR_BYTES * currentEdgeLength);
    *edge = (void *)e;
}

void *statCopyEdgeInitializeEmpty(const void *edge)
{
    StatBitVectorEdge_t *ret;

    ret = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
    if (ret == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for edge copy\n", sizeof(StatBitVectorEdge_t));
        return NULL;
    }
    ret->length = statGraphRoutinesTotalWidth;
    ret->bitVector = (StatBitVector_t *)calloc(ret->length, STAT_BITVECTOR_BYTES);
    if (ret->bitVector == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for bit vector\n", ret->length * STAT_BITVECTOR_BYTES);
        return NULL;
    }
    return (void *)ret;
}

int bitVectorContains(StatBitVector_t *vec, int val)
{
    return !!(vec[val / STAT_BITVECTOR_BITS] & STAT_GRAPH_BIT(val % STAT_BITVECTOR_BITS));
}

void *statMergeEdgeOrdered(void *edge1, const void *edge2)
{
    unsigned int i;
    int bit, byte;

    StatBitVectorEdge_t *e1 = (StatBitVectorEdge_t *)edge1, *e2 = (StatBitVectorEdge_t *)edge2;

    for (i = 0; i < statGraphRoutinesRanksListLength; i++)
    {
        if (bitVectorContains(e2->bitVector, statGraphRoutinesCurrentIndex * STAT_BITVECTOR_BITS + i) == 1)
        {
            byte = statGraphRoutinesRanksList[i] / STAT_BITVECTOR_BITS;
            bit = statGraphRoutinesRanksList[i] % STAT_BITVECTOR_BITS;
            e1->bitVector[byte] |= STAT_GRAPH_BIT(bit);
        }
    }
    return edge1;
}

void statSerializeCountRepEdge(char *buf, const void *edge)
{
    memcpy(buf, edge, sizeof(StatCountRepEdge_t));
}

unsigned int statSerializeCountRepEdgeLength(const void *edge)
{
    return sizeof(StatCountRepEdge_t);
}

void statDeserializeCountRepEdge(void **edge, const char *buf, unsigned int bufLength)
{
    StatCountRepEdge_t *e;

    e = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
    memcpy(e, buf, bufLength);
    *edge = (void *)e;
}

char *statCountRepEdgeToText(const void *edge)
{
    char *ret;
    StatCountRepEdge_t *e;

    e = (StatCountRepEdge_t *)edge;
    ret = (char *)malloc(STAT_GRAPH_CHUNK * sizeof(char));
    snprintf(ret, STAT_GRAPH_CHUNK, "%lld:[%lld](%lld)", e->count, e->representative, e->checksum);
    return ret;
}

void *statMergeCountRepEdge(void *edge1, const void *edge2)
{
    StatCountRepEdge_t *e1, *e2;

    e1 = (StatCountRepEdge_t *)edge1;
    e2 = (StatCountRepEdge_t *)edge2;
    e1->count += e2->count;
    e1->checksum += e2->checksum;
    if (e2->representative < e1->representative)
        e1->representative = e2->representative;
    return edge1;
}

void *statCopyCountRepEdge(const void *edge)
{
    StatCountRepEdge_t *ret;

    ret = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
    memcpy((void *)ret, edge, sizeof(StatCountRepEdge_t));
    return (void *)ret;
}

void statFreeCountRepEdge(void *edge)
{
    free((StatCountRepEdge_t *) edge);
}

long statCountRepEdgeCheckSum(const void *edge)
{
    return ((StatCountRepEdge_t *)edge)->checksum;
}
