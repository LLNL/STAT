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

//! the bit vector routines
graphlib_functiontable_p gStatBitVectorFunctions;

//! the bit vector filter routines to append bit vectors
graphlib_functiontable_p gStatMergeFunctions;

//! the bit vector to reorder the bit vectors by MPI rank
graphlib_functiontable_p gStatReorderFunctions;

//! the count and representative routines
graphlib_functiontable_p gStatCountRepFunctions;

//! the final bit vector width
int gStatGraphRoutinesTotalWidth;

//! the input list of bit vector widths
int *gStatGraphRoutinesEdgeLabelWidths;

//! the current index into the bit vector
int gStatGraphRoutinesCurrentIndex;

//! the ranks list for the current bit vector
int *gStatGraphRoutinesRanksList;

//! the length of the ranks list
int gStatGraphRoutinesRanksListLength;

unsigned int statStringHash(const char *str)
{
    unsigned int hash = 0;
    int c;

    /* perform a sdbm hash */
    while (c = *str++)
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

graphlib_graph_p createRootedGraph(unsigned int sampleType)
{
    graphlib_error_t graphlibError;
    graphlib_graph_p retGraph = NULL;

    if (sampleType & STAT_SAMPLE_COUNT_REP)
        graphlibError = graphlib_newGraph(&retGraph, gStatCountRepFunctions);
    else
        graphlibError = graphlib_newGraph(&retGraph, gStatBitVectorFunctions);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error creating new graph\n");
        return NULL;
    }

    graphlib_nodeattr_t nodeAttr = {1, 0, 20, GRC_LIGHTGREY, 0, 0, (char *)"/", -1};
    graphlibError = graphlib_addNode(retGraph, 0, &nodeAttr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error adding sentinel node to graph\n");
        return NULL;
    }
    return retGraph;
}

StatBitVectorEdge_t *initializeBitVectorEdge(int numTasks)
{
    StatBitVectorEdge_t *edge;
    
    edge = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
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
    gStatBitVectorFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (gStatBitVectorFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    gStatBitVectorFunctions->serialize_node = statSerializeNode;
    gStatBitVectorFunctions->serialize_node_length = statSerializeNodeLength;
    gStatBitVectorFunctions->deserialize_node = statDeserializeNode;
    gStatBitVectorFunctions->node_to_text = statNodeToText;
    gStatBitVectorFunctions->merge_node = statMergeNode;
    gStatBitVectorFunctions->copy_node = statCopyNode;
    gStatBitVectorFunctions->free_node = statFreeNode;
    gStatBitVectorFunctions->serialize_edge = statSerializeEdge;
    gStatBitVectorFunctions->serialize_edge_length = statSerializeEdgeLength;
    gStatBitVectorFunctions->deserialize_edge = statDeserializeEdge;
    gStatBitVectorFunctions->edge_to_text = statEdgeToText;
    gStatBitVectorFunctions->merge_edge = statMergeEdge;
    gStatBitVectorFunctions->copy_edge = statCopyEdge;
    gStatBitVectorFunctions->free_edge = statFreeEdge;
    gStatBitVectorFunctions->edge_checksum = statEdgeCheckSum;
}

void statFreeBitVectorFunctions()
{
    if (gStatBitVectorFunctions != NULL)
        free(gStatBitVectorFunctions);
    gStatBitVectorFunctions = NULL;
}

void statInitializeMergeFunctions()
{
    statFreeMergeFunctions();
    gStatMergeFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (gStatMergeFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    gStatMergeFunctions->serialize_node = statSerializeNode;
    gStatMergeFunctions->serialize_node_length = statSerializeNodeLength;
    gStatMergeFunctions->deserialize_node = statDeserializeNode;
    gStatMergeFunctions->node_to_text = statNodeToText;
    gStatMergeFunctions->merge_node = statMergeNode;
    gStatMergeFunctions->copy_node = statCopyNode;
    gStatMergeFunctions->free_node = statFreeNode;
    gStatMergeFunctions->serialize_edge = statSerializeEdge;
    gStatMergeFunctions->serialize_edge_length = statSerializeEdgeLength;
    gStatMergeFunctions->deserialize_edge = statFilterDeserializeEdge;
    gStatMergeFunctions->edge_to_text = statEdgeToText;
    gStatMergeFunctions->merge_edge = statMergeEdge;
    gStatMergeFunctions->copy_edge = statCopyEdge;
    gStatMergeFunctions->free_edge = statFreeEdge;
    gStatMergeFunctions->edge_checksum = statEdgeCheckSum;
}

void statFreeMergeFunctions()
{
    if (gStatMergeFunctions != NULL)
        free(gStatMergeFunctions);
    gStatMergeFunctions = NULL;
}

void statInitializeReorderFunctions()
{
    statFreeReorderFunctions();
    gStatReorderFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (gStatReorderFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    gStatReorderFunctions->serialize_node = statSerializeNode;
    gStatReorderFunctions->serialize_node_length = statSerializeNodeLength;
    gStatReorderFunctions->deserialize_node = statDeserializeNode;
    gStatReorderFunctions->node_to_text = statNodeToText;
    gStatReorderFunctions->merge_node = statMergeNode;
    gStatReorderFunctions->copy_node = statCopyNode;
    gStatReorderFunctions->free_node = statFreeNode;
    gStatReorderFunctions->serialize_edge = statSerializeEdge;
    gStatReorderFunctions->serialize_edge_length = statSerializeEdgeLength;
    gStatReorderFunctions->deserialize_edge = statDeserializeEdge;
    gStatReorderFunctions->edge_to_text = statEdgeToText;
    gStatReorderFunctions->merge_edge = statMergeEdgeOrdered;
    gStatReorderFunctions->copy_edge = statCopyEdgeInitializeEmpty;
    gStatReorderFunctions->free_edge = statFreeEdge;
    gStatReorderFunctions->edge_checksum = statEdgeCheckSum;
}

void statFreeReorderFunctions()
{
    if (gStatReorderFunctions != NULL)
        free(gStatReorderFunctions);
    gStatReorderFunctions = NULL;
}

void statInitializeCountRepFunctions()
{
    statFreeCountRepFunctions();
    gStatCountRepFunctions = (graphlib_functiontable_p)malloc(sizeof(graphlib_functiontable_t));
    if (gStatCountRepFunctions == NULL)
        fprintf(stderr, "Failed to malloc function table\n");
    gStatCountRepFunctions->serialize_node = statSerializeNode;
    gStatCountRepFunctions->serialize_node_length = statSerializeNodeLength;
    gStatCountRepFunctions->deserialize_node = statDeserializeNode;
    gStatCountRepFunctions->node_to_text = statNodeToText;
    gStatCountRepFunctions->merge_node = statMergeNode;
    gStatCountRepFunctions->copy_node = statCopyNode;
    gStatCountRepFunctions->free_node = statFreeNode;
    gStatCountRepFunctions->serialize_edge = statSerializeCountRepEdge;
    gStatCountRepFunctions->serialize_edge_length = statSerializeCountRepEdgeLength;
    gStatCountRepFunctions->deserialize_edge = statDeserializeCountRepEdge;
    gStatCountRepFunctions->edge_to_text = statCountRepEdgeToText;
    gStatCountRepFunctions->merge_edge = statMergeCountRepEdge;
    gStatCountRepFunctions->copy_edge = statCopyCountRepEdge;
    gStatCountRepFunctions->free_edge = statFreeCountRepEdge;
    gStatCountRepFunctions->edge_checksum = statCountRepEdgeCheckSum;
}

void statFreeCountRepFunctions()
{
    if (gStatCountRepFunctions != NULL)
        free(gStatCountRepFunctions);
    gStatCountRepFunctions = NULL;
}

size_t statBitVectorLength(int numTasks)
{
    int intRet;

    intRet = numTasks / STAT_BITVECTOR_BITS;
    if (numTasks % STAT_BITVECTOR_BITS != 0)
        intRet += 1;
    return intRet;
}

void statSerializeNode(char *buf, const void *node)
{
  if (node !=  NULL)
    strcpy(buf, (char *)node);
}

unsigned int statSerializeNodeLength(const void *node)
{
  if (node != NULL)
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
    char *ptr;
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;

    ptr = buf;
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
    char *ptr;
    StatBitVectorEdge_t *e;

    ptr = (char *)buf;
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
    int i, j, inRange = 0, firstIteration = 1, currentValue, lastValue = 0;
    unsigned int charRetSize = 0, count = 0;
    char val[128], *charRet;
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;

    charRet = (char *)malloc(STAT_GRAPH_CHUNK * sizeof(char));
    charRetSize = STAT_GRAPH_CHUNK;
    if (charRet == NULL)
    {
        fprintf(stderr, "%s: Failed to allocte memory for edge label\n", strerror(errno));
        return NULL;
    }
    sprintf(charRet + count, "[");
    count += 1;
    for (i = 0; i < e->length; i++)
    {
        if (charRetSize - count < 1024)
        {
            /* Reallocate if we are within 1024 bytes of the end */
            /* This is a large threshold to keep it out of the inner loop */
            charRetSize += STAT_GRAPH_CHUNK;
            charRet = (char *)realloc(charRet, charRetSize * sizeof(char));
            if (charRet == NULL)
            {
                fprintf(stderr, "%s: Failed to reallocte %d bytes of memory for edge label\n", strerror(errno), charRetSize);
                return NULL;
            }
        }
        for (j = 0; j < 8 * sizeof(StatBitVector_t); j++)
        {
            if (e->bitVector[i] & STAT_GRAPH_BIT(j))
            {
                currentValue = i * 8 * sizeof(StatBitVector_t) + j;
                if (inRange == 0)
                {
                    snprintf(val, 128, "%d", currentValue);
                    if (firstIteration == 0)
                    {
                        if (currentValue == lastValue + 1)
                        {
                            inRange = 1;
                            sprintf(charRet + count, "-");
                            count += 1;
                        }
                        else
                        {
                            sprintf(charRet + count, ",");
                            count += 1;
                            sprintf(charRet + count, "%s", val);
                            count += strlen(val);
                        }
                    }
                    else
                    {
                        sprintf(charRet + count, "%s", val);
                        count += strlen(val);
                    }
                }
                else
                {
                    if (currentValue != lastValue + 1)
                    {
                        snprintf(val, 128, "%d,%d", lastValue, currentValue);
                        sprintf(charRet + count, "%s", val);
                        count += strlen(val);
                        inRange = 0;
                    }
                }
                firstIteration = 0;
                lastValue = currentValue;
            }
        }
    }
    if (inRange == 1)
    {
        snprintf(val, 128, "%d", lastValue);
        sprintf(charRet + count, "%s", val);
        count += strlen(val);
    }
    sprintf(charRet + count, "]");
    count += 1;

    return charRet;
}

void *statMergeEdge(void *edge1, const void *edge2)
{
    unsigned int i;
    size_t length;
    StatBitVectorEdge_t *e1, *e2;
    
    e1 = (StatBitVectorEdge_t *)edge1;
    e2 = (StatBitVectorEdge_t *)edge2;

    length = e1->length;
    if (e2->length < e1->length)
        length = e2->length;
    for (i = 0; i < length; i++)
        e1->bitVector[i] |= e2->bitVector[i];
    return edge1;
}

void *statCopyEdge(const void *edge)
{
    StatBitVectorEdge_t *e, *bvRet;
    
    e = (StatBitVectorEdge_t *)edge;
    bvRet = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
    if (bvRet == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for edge copy\n", sizeof(StatBitVectorEdge_t));
        return NULL;
    }
    bvRet->length = e->length;
    bvRet->bitVector = (StatBitVector_t *)malloc(e->length * STAT_BITVECTOR_BYTES);
    if (bvRet->bitVector == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for bit vector\n", e->length * STAT_BITVECTOR_BYTES);
        return NULL;
    }
    memcpy(bvRet->bitVector, e->bitVector, STAT_BITVECTOR_BYTES * e->length);
    return (void *)bvRet;
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
    int i;
    long longRet = 0;
    StatBitVectorEdge_t *e = (StatBitVectorEdge_t *)edge;

    for (i = 0; i < e->length; i++)
        longRet = longRet + e->bitVector[i] * (e->length - i + 1);

    return longRet;
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
    e->length = gStatGraphRoutinesTotalWidth;
    e->bitVector = (StatBitVector_t *)calloc(e->length, STAT_BITVECTOR_BYTES);
    if (e == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for deserialized edge bit vector\n", STAT_BITVECTOR_BYTES * e->length);
        return;
    }

    offset = 0;
    for (i = 0; i < gStatGraphRoutinesCurrentIndex; i++)
        offset += gStatGraphRoutinesEdgeLabelWidths[i];

    memcpy((void *)&(e->bitVector[offset]), ptr, STAT_BITVECTOR_BYTES * currentEdgeLength);
    *edge = (void *)e;
}

void *statCopyEdgeInitializeEmpty(const void *edge)
{
    StatBitVectorEdge_t *bvRet;

    bvRet = (StatBitVectorEdge_t *)malloc(sizeof(StatBitVectorEdge_t));
    if (bvRet == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for edge copy\n", sizeof(StatBitVectorEdge_t));
        return NULL;
    }
    bvRet->length = gStatGraphRoutinesTotalWidth;
    bvRet->bitVector = (StatBitVector_t *)calloc(bvRet->length, STAT_BITVECTOR_BYTES);
    if (bvRet->bitVector == NULL)
    {
        fprintf(stderr, "Failed to allocate %u bytes for bit vector\n", bvRet->length * STAT_BITVECTOR_BYTES);
        return NULL;
    }
    return (void *)bvRet;
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

    for (i = 0; i < gStatGraphRoutinesRanksListLength; i++)
    {
        if (bitVectorContains(e2->bitVector, gStatGraphRoutinesCurrentIndex * STAT_BITVECTOR_BITS + i) == 1)
        {
            byte = gStatGraphRoutinesRanksList[i] / STAT_BITVECTOR_BITS;
            bit = gStatGraphRoutinesRanksList[i] % STAT_BITVECTOR_BITS;
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
    char *charRet;
    StatCountRepEdge_t *e;

    e = (StatCountRepEdge_t *)edge;
    charRet = (char *)malloc(STAT_GRAPH_CHUNK * sizeof(char));
    snprintf(charRet, STAT_GRAPH_CHUNK, "%lld:[%lld](%lld)", e->count, e->representative, e->checksum);
    return charRet;
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
    StatCountRepEdge_t *crRet;

    crRet = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
    memcpy((void *)crRet, edge, sizeof(StatCountRepEdge_t));
    return (void *)crRet;
}

void statFreeCountRepEdge(void *edge)
{
    free((StatCountRepEdge_t *) edge);
}

long statCountRepEdgeCheckSum(const void *edge)
{
    return ((StatCountRepEdge_t *)edge)->checksum;
}

StatCountRepEdge_t *getBitVectorCountRep(StatBitVectorEdge_t *edge)
{
    unsigned int i, j, rank;
    StatCountRepEdge_t *crRet;

    crRet = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
    if (crRet == NULL)
        return NULL;
    crRet->count = 0;
    crRet->representative = -1;
    crRet->checksum = 0;

    for (i = 0, rank = 0; i < edge->length; i++)
    {
        for (j = 0; j < STAT_BITVECTOR_BITS; j++, rank++)
        {
            if (edge->bitVector[i] & STAT_GRAPH_BIT(j))
            {
                if (crRet->representative == -1)
                    crRet->representative = rank;
                crRet->count += 1;
                crRet->checksum += rank + 1;
            }
        }
    }

    return crRet;
}
