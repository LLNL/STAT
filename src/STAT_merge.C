/*
Copyright (c) 2007-2017, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee [lee218@llnl.gov], Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, Martin Schulz, Niklas Nielson, Nicklas Bo Jensen, Jesper Nielson, and Sven Karlsson.
LLNL-CODE-727016.
All rights reserved.

This file is part of STAT. For details, see http://www.github.com/LLNL/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <Python.h>
#include "graphlib.h"
#include <vector>
#include <errno.h>
#include "STAT_GraphRoutines.h"

using namespace std;

extern "C" {

//! a vector of generated graphs
vector<graphlib_graph_p> *gGraphs = NULL;

//! the highest rank task to represent
int gHighRank;

#ifdef COUNTREP
//! the count and representative routines
extern graphlib_functiontable_p gStatCountRepFunctions;
#else
//! the bit vector routines
extern graphlib_functiontable_p gStatBitVectorFunctions;
#endif

//! Initialize graphlib and set the graph routines
static PyObject *py_Init_Graphlib(PyObject *self, PyObject *args)
{
    graphlib_error_t graphlibError;

    if (!PyArg_ParseTuple(args, "i", &gHighRank))
    {
        fprintf(stderr, "Failed to parse args, expecting (int)\n");
        return Py_BuildValue("i", -1);
    }
    graphlibError = graphlib_Init();
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Failed to init graphlib\n");
        return Py_BuildValue("i", -1);
    }

#ifdef COUNTREP
    statInitializeCountRepFunctions();
#else
    statInitializeBitVectorFunctions();
#endif

    return Py_BuildValue("i", 0);
}

//! Create a new graph and return the handle
static PyObject *py_newGraph(PyObject *self, PyObject *args)
{
    int handle;
    graphlib_graph_p newGraph;
    graphlib_error_t graphlibError;

#ifdef COUNTREP
    graphlibError = graphlib_newGraph(&newGraph, gStatCountRepFunctions);
#else
    graphlibError = graphlib_newGraph(&newGraph, gStatBitVectorFunctions);
#endif
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error intializing graph\n");
        return Py_BuildValue("i", -1);
    }

    if (gGraphs == NULL)
        gGraphs = new vector<graphlib_graph_p>;
    gGraphs->push_back(newGraph);
    handle = gGraphs->size() - 1;

    return Py_BuildValue("i", handle);
}

//! Add a stack trace to a previously generated graph
static PyObject *py_Add_Trace(PyObject *self, PyObject *args)
{
    int i, task, count, size, nodeId, prevId, handle, bit, byte;
    char *trace, *ptr, *next, *tmp;
    char path[BUFSIZE], prevPath[BUFSIZE];
#ifdef COUNTREP
    StatCountRepEdge_t *edge = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
#else
    StatBitVectorEdge_t *edge;
#endif
    graphlib_graph_p cur_graph, graphPtr = NULL;
    graphlib_error_t graphlibError;
    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0,NULL,1};
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0};

    if (!PyArg_ParseTuple(args, "iiis", &handle, &task, &count, &trace))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, int, string)\n");
        return Py_BuildValue("i", -1);
    }

    graphPtr = (*gGraphs)[handle];

#ifdef COUNTREP
    edge = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
    if (edge == NULL)
    {
        fprintf(stderr, "Failed to create bit edge\n");
        return Py_BuildValue("i", -1);
    }
    edge->count = 1;
    edge->representative = task;
    edge->checksum = task;
    edgeAttr.label = (void *)edge;
    graphlibError = graphlib_newGraph(&cur_graph, gStatCountRepFunctions);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Failed to create new graph\n");
        return Py_BuildValue("i", -1);
    }
#else
    edge = initializeBitVectorEdge(gHighRank);
    if (edge == NULL)
    {
        fprintf(stderr, "Failed to create bit edge\n");
        return Py_BuildValue("i", -1);
    }

    byte = task / STAT_BITVECTOR_BITS;
    bit = task % STAT_BITVECTOR_BITS;
    edge->bitVector[byte] |= STAT_GRAPH_BIT(bit);

    edgeAttr.label = (void *)edge;

    graphlibError = graphlib_newGraph(&cur_graph, gStatBitVectorFunctions);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Failed to create new graph\n");
        return Py_BuildValue("i", -1);
    }
#endif

    tmp = (char *)malloc(2 * sizeof(char));
    snprintf(tmp, 2, "/");
    nodeAttr.label = (void *)tmp;
    snprintf(path, BUFSIZE, "%s", (char *)nodeAttr.label);
    nodeId = 0;
    prevId = 0;
    graphlibError = graphlib_addNode(cur_graph, nodeId, &nodeAttr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Failed to add node\n");
        return Py_BuildValue("i", -1);
    }

    next = trace;
    ptr = trace;
    for (i = 0; i < count; i++)
    {
        size = 1;
        ptr = next;
        while (*ptr != '\n')
        {
            size++;
            ptr += 1;
        }
        tmp = (char *)malloc((size + 1) * sizeof(char));
        snprintf(tmp, size, "%s", next);
        nodeAttr.label = (void *)tmp;
        next += size;
        snprintf(prevPath, BUFSIZE, "%s", path);
        snprintf(path, BUFSIZE, "%s%s", prevPath, (char *)nodeAttr.label);
        nodeId = statStringHash(path);

        graphlibError = graphlib_addNode(cur_graph, nodeId, &nodeAttr);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            fprintf(stderr, "Failed to add node\n");
            return Py_BuildValue("i", -1);
        }

        graphlibError = graphlib_addDirectedEdge(cur_graph, prevId, nodeId, &edgeAttr);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            fprintf(stderr, "Failed to add edge\n");
            return Py_BuildValue("i", -1);
        }
        prevId = nodeId;
    }

    graphlibError = graphlib_mergeGraphs(graphPtr, cur_graph);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error merging graph\n");
        return Py_BuildValue("i", -1);
    }

#ifdef COUNTREP
    graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeCountRepEdge);
#else
    graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeEdge);
#endif
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error deleting edge attr\n");
        return Py_BuildValue("i", -1);
    }

    graphlibError = graphlib_delGraph(cur_graph);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error deleting graph\n");
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

//! Merge 2 graphs
static PyObject *py_Merge_Traces(PyObject *self, PyObject *args)
{
    int handle1, handle2;
    graphlib_graph_p cur_graph, graphPtr;
    graphlib_error_t graphlibError;

    if (!PyArg_ParseTuple(args, "ii", &handle1, &handle2))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, int, string)\n");
        return Py_BuildValue("i", -1);
    }
    graphPtr = (*gGraphs)[handle1];
    cur_graph = (*gGraphs)[handle2];

    graphlibError = graphlib_mergeGraphs(graphPtr, cur_graph);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error merging graph\n");
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

//! Serialze the graph to a file
static PyObject *py_Serialize_Graph(PyObject *self, PyObject *args)
{
    char *buf, *fileName;
    int handle, ret;
    uint64_t bufLen;
    FILE *f;
    graphlib_graph_p graphPtr = NULL;
    graphlib_error_t graphlibError;

    if (!PyArg_ParseTuple(args, "is", &handle, &fileName))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, string)\n");
        return Py_BuildValue("i", -1);
    }

    graphPtr = (*gGraphs)[handle];

    graphlibError = graphlib_serializeBasicGraph(graphPtr, &buf, &bufLen);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "%d Error serializing graph %d\n", graphlibError, handle);
        return Py_BuildValue("i", -1);
    }

    f = fopen(fileName, "w");
    if (f == NULL)
    {
        fprintf(stderr, "%s: Error opening file %s\n", strerror(errno), fileName);
        return Py_BuildValue("i", -1);
    }

    ret = fwrite(buf, sizeof(char), bufLen, f);
    if (ret != (int)bufLen)
    {
        fprintf(stderr, "%s: %d Error writing serialized graph %d to file %s\n", strerror(errno), ret, handle, fileName);
        return Py_BuildValue("i", -1);
    }

    ret = fclose(f);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error closing file %s\n", strerror(errno), ret, fileName);
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

//! Deserialize the graph from a file and return its handle
static PyObject *py_Deserialize_Graph(PyObject *self, PyObject *args)
{
    char *buf, *fileName;
    long bufLen;
    int handle, ret;
    FILE *f;
    graphlib_graph_p graphPtr = NULL;
    graphlib_error_t graphlibError;

    if (!PyArg_ParseTuple(args, "s", &fileName))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, string)\n");
        return Py_BuildValue("i", -1);
    }

    f = fopen(fileName, "r");
    if (f == NULL)
    {
        fprintf(stderr, "%s: Error opening file %s\n", strerror(errno), fileName);
        return Py_BuildValue("i", -1);
    }

    ret = fseek(f, 0, SEEK_END);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error seeking file %s\n", strerror(errno), ret, fileName);
        return Py_BuildValue("i", -1);
    }

    bufLen = ftell(f);
    if (bufLen < 0)
    {
        fprintf(stderr, "%s: %ld Error ftell file %s\n", strerror(errno), bufLen, fileName);
        return Py_BuildValue("i", -1);
    }

    ret = fseek(f, 0, SEEK_SET);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error seeking file %s\n", strerror(errno), ret, fileName);
        return Py_BuildValue("i", -1);
    }

    buf = (char *)malloc(bufLen * sizeof(char));
    if (buf == NULL)
    {
        fprintf(stderr, "%s: Error allocating %ld bytes for file %s\n", strerror(errno), bufLen, fileName);
        return Py_BuildValue("i", -1);
    }

    ret = fread(buf, bufLen, 1, f);
    if (ret != 1)
    {
        fprintf(stderr, "%s: Error reading serialized graph from file %s.  %d of %ld bytes read\n", strerror(errno), fileName, ret, bufLen);
        return Py_BuildValue("i", -1);
    }

    ret = fclose(f);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error closing file %s\n", strerror(errno), ret, fileName);
        return Py_BuildValue("i", -1);
    }

#ifdef COUNTREP
    graphlibError = graphlib_deserializeBasicGraph(&graphPtr, gStatCountRepFunctions, buf, (unsigned int)bufLen);
#else
    graphlibError = graphlib_deserializeBasicGraph(&graphPtr, gStatBitVectorFunctions, buf, (unsigned int)bufLen);
#endif
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "Error serializing graph\n");
        return Py_BuildValue("i", -1);
    }

    if (gGraphs == NULL)
        gGraphs = new vector<graphlib_graph_p>;
    gGraphs->push_back(graphPtr);
    handle = gGraphs->size() - 1;

    return Py_BuildValue("i", handle);
}

//! Output a graph to a .dot file
static PyObject *py_Output_Graph(PyObject *self, PyObject *args)
{
    char *fileName;
    int handle;
    graphlib_error_t graphlibError;
    graphlib_graph_p graphPtr = NULL;

    if (!PyArg_ParseTuple(args, "is", &handle, &fileName))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, string)\n");
        return Py_BuildValue("i", -1);
    }

    graphPtr = (*gGraphs)[handle];

    graphlibError = graphlib_colorGraphByLeadingEdgeLabel(graphPtr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(stderr, "graphlib error coloring graph by leading edge label\n");
        return Py_BuildValue("i", -1);
    }

    graphlibError =  graphlib_scaleNodeWidth(graphPtr, 80, 160);
    if ( GRL_IS_FATALERROR(graphlibError) )
    {
        fprintf(stderr, "graphlib error scaling node width\n");
        return Py_BuildValue("i", -1);
    }

    graphlibError = graphlib_exportGraph(fileName, GRF_DOT, graphPtr);
    if ( GRL_IS_FATALERROR(graphlibError) )
    {
        fprintf(stderr, "graphlib error exporting graph\n");
        return Py_BuildValue("i", -1);
    }

    graphlibError = graphlib_delGraph(graphPtr);
    if ( GRL_IS_FATALERROR(graphlibError) )
    {
        fprintf(stderr, "graphlib error deleting graph\n");
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

//! The Python method table for this module
static PyMethodDef _STATmergeMethods[] = {
    {"Init_Graphlib", py_Init_Graphlib, METH_VARARGS, "graphlib init."},
    {"New_Graph", py_newGraph, METH_VARARGS, "create new graph."},
    {"Add_Trace", py_Add_Trace, METH_VARARGS, "trace generator."},
    {"Merge_Traces", py_Merge_Traces, METH_VARARGS, "trace merger."},
    {"Serialize_Graph", py_Serialize_Graph, METH_VARARGS, "serialize graph."},
    {"Deserialize_Graph", py_Deserialize_Graph, METH_VARARGS, "deserialize graph."},
    {"Output_Graph", py_Output_Graph, METH_VARARGS, "graph exporter."},
    {NULL, NULL, 0, NULL}
};

//! The Python initialization routine for this module
void init_STATmerge()
{
    Py_InitModule("_STATmerge", _STATmergeMethods);
}

}
