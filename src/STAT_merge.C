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

#include <Python.h>
#include "graphlib.h"
#include <vector>
#include <errno.h>
#include "STAT_GraphRoutines.h"

#define BUFSIZE 16384

using namespace std;
extern "C" {

vector<graphlib_graph_p> *graphs = NULL;
int high_rank;
#ifdef COUNTREP
extern graphlib_functiontable_p statCountRepFunctions;
#else
extern graphlib_functiontable_p statBitVectorFunctions;
#endif

static PyObject *py_Init_Graphlib(PyObject *self, PyObject *args)
{
    graphlib_error_t gl_err;

    if (!PyArg_ParseTuple(args, "i", &high_rank))
    {
        fprintf(stderr, "Failed to parse args, expecting (int)\n");
        return Py_BuildValue("i", -1);
    }
    gl_err = graphlib_Init();
    if (GRL_IS_FATALERROR(gl_err))
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

static PyObject *py_New_Graph(PyObject *self, PyObject *args)
{
    int handle;
    graphlib_graph_p new_graph;
    graphlib_error_t gl_err;

#ifdef COUNTREP
    gl_err = graphlib_newGraph(&new_graph, statCountRepFunctions);
#else
    gl_err = graphlib_newGraph(&new_graph, statBitVectorFunctions);
#endif
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Error intializing graph\n");
        return Py_BuildValue("i", -1);
    }

    if (graphs == NULL)
        graphs = new vector<graphlib_graph_p>;
    graphs->push_back(new_graph);
    handle = graphs->size() - 1;

    return Py_BuildValue("i", handle);
}

static PyObject *py_Add_Trace(PyObject *self, PyObject *args)
{
    int depth, i, task, count, size;
    int node_id, prev_id, handle;
    char *trace, *ptr, *next, *tmp;
    char path[BUFSIZE], prev_path[BUFSIZE];
#ifdef COUNTREP
    StatCountRepEdge_t *edge = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
#else
    StatBitVectorEdge_t *edge;
#endif
    graphlib_graph_p cur_graph, graph_ptr = NULL;
    graphlib_error_t gl_err;
    graphlib_nodeattr_t node_attr = {1,0,20,GRC_LIGHTGREY,0,0,NULL,1};
    graphlib_edgeattr_t edge_attr = {1,0,NULL,0,0,0};

    snprintf(path, BUFSIZE, "");
    if (!PyArg_ParseTuple(args, "iiis", &handle, &task, &count, &trace))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, int, string)\n");
        return Py_BuildValue("i", -1);
    }

    graph_ptr = (*graphs)[handle];

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
    edge_attr.label = (void *)edge;
    gl_err = graphlib_newGraph(&cur_graph, statCountRepFunctions);
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Failed to create new graph\n");
        return Py_BuildValue("i", -1);
    }
#else
    edge = initializeBitVectorEdge(high_rank);
    if (edge == NULL)
    {
        fprintf(stderr, "Failed to create bit edge\n");
        return Py_BuildValue("i", -1);
    }

    int byte = task / STAT_BITVECTOR_BITS;
    int bit = task % STAT_BITVECTOR_BITS;
    edge->bitVector[byte] |= STAT_GRAPH_BIT(bit);

    edge_attr.label = (void *)edge;

    gl_err = graphlib_newGraph(&cur_graph, statBitVectorFunctions);
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Failed to create new graph\n");
        return Py_BuildValue("i", -1);
    }
#endif

    tmp = (char *)malloc(2 * sizeof(char));
    snprintf(tmp, 2, "/");
    node_attr.label = (void *)tmp;
    snprintf(path, BUFSIZE, "%s", node_attr.label);
    node_id = 0;
    prev_id = 0;
    gl_err = graphlib_addNode(cur_graph, node_id, &node_attr);
    if (GRL_IS_FATALERROR(gl_err))
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
        node_attr.label = (void *)tmp;
        next += size;
        snprintf(prev_path, BUFSIZE, "%s", path);
        snprintf(path, BUFSIZE, "%s%s", prev_path, node_attr.label);
        node_id = statStringHash(path);

        gl_err = graphlib_addNode(cur_graph, node_id, &node_attr);
        if (GRL_IS_FATALERROR(gl_err))
        {
            fprintf(stderr, "Failed to add node\n");
            return Py_BuildValue("i", -1);
        }

        gl_err = graphlib_addDirectedEdge(cur_graph, prev_id, node_id, &edge_attr);
        if (GRL_IS_FATALERROR(gl_err))
        {
            fprintf(stderr, "Failed to add edge\n");
            return Py_BuildValue("i", -1);
        }
        prev_id = node_id;
    }

    gl_err = graphlib_mergeGraphs(graph_ptr, cur_graph);
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Error merging graph\n");
        return Py_BuildValue("i", -1);
    }

#ifdef COUNTREP
    gl_err = graphlib_delEdgeAttr(edge_attr, statFreeCountRepEdge);
#else
    gl_err = graphlib_delEdgeAttr(edge_attr, statFreeEdge);
#endif
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Error deleting edge attr\n");
        return Py_BuildValue("i", -1);
    }

    gl_err = graphlib_delGraph(cur_graph);
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Error deleting graph\n");
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

static PyObject *py_Merge_Traces(PyObject *self, PyObject *args)
{
    int handle1, handle2;
    graphlib_graph_p cur_graph, graph_ptr;
    graphlib_error_t gl_err;

    if (!PyArg_ParseTuple(args, "ii", &handle1, &handle2))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, int, string)\n");
        return Py_BuildValue("i", -1);
    }
    graph_ptr = (*graphs)[handle1];
    cur_graph = (*graphs)[handle2];

    gl_err = graphlib_mergeGraphs(graph_ptr, cur_graph);
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Error merging graph\n");
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

static PyObject *py_Serialize_Graph(PyObject *self, PyObject *args)
{
    char *buf, *filename;
    int handle, ret;
    unsigned long buf_len;
    FILE *f;
    graphlib_graph_p graph_ptr = NULL;
    graphlib_error_t gl_err;

    if (!PyArg_ParseTuple(args, "is", &handle, &filename))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, string)\n");
        return Py_BuildValue("i", -1);
    }

    graph_ptr = (*graphs)[handle];

    gl_err = graphlib_serializeBasicGraph(graph_ptr, &buf, &buf_len);
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "%d Error serializing graph %d\n", gl_err, handle);
        return Py_BuildValue("i", -1);
    }

    f = fopen(filename, "w");
    if (f == NULL)
    {
        fprintf(stderr, "%s: Error opening file %s\n", strerror(errno), filename);
        return Py_BuildValue("i", -1);
    }

    ret = fwrite(buf, sizeof(char), buf_len, f);
    if (ret != buf_len)
    {
        fprintf(stderr, "%s: %d Error writing serialized graph %d to file %s\n", strerror(errno), ret, handle, filename);
        return Py_BuildValue("i", -1);
    }

    ret = fclose(f);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error closing file %s\n", strerror(errno), ret, filename);
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

static PyObject *py_Deserialize_Graph(PyObject *self, PyObject *args)
{
    char *buf, *filename;
    long buf_len;
    int handle, ret;
    FILE *f;
    graphlib_graph_p graph_ptr = NULL;
    graphlib_error_t gl_err;

    if (!PyArg_ParseTuple(args, "s", &filename))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, string)\n");
        return Py_BuildValue("i", -1);
    }

    f = fopen(filename, "r");
    if (f == NULL)
    {
        fprintf(stderr, "%s: Error opening file %s\n", strerror(errno), filename);
        return Py_BuildValue("i", -1);
    }

    ret = fseek(f, 0, SEEK_END);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error seeking file %s\n", strerror(errno), ret, filename);
        return Py_BuildValue("i", -1);
    }

    buf_len = ftell(f);
    if (buf_len < 0)
    {
        fprintf(stderr, "%s: %d Error ftell file %s\n", strerror(errno), buf_len, filename);
        return Py_BuildValue("i", -1);
    }

    ret = fseek(f, 0, SEEK_SET);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error seeking file %s\n", strerror(errno), ret, filename);
        return Py_BuildValue("i", -1);
    }

    buf = (char *)malloc(buf_len * sizeof(char));
    if (buf == NULL)
    {
        fprintf(stderr, "%s: Error allocating %d bytes for file %s\n", strerror(errno), buf_len, filename);
        return Py_BuildValue("i", -1);
    }

    ret = fread(buf, buf_len, 1, f);
    if (ret != 1)
    {
        fprintf(stderr, "%s: %d Error reading serialized graph from file %s.  %d of %d bytes read\n", strerror(errno), handle, filename, ret, buf_len);
        return Py_BuildValue("i", -1);
    }

    ret = fclose(f);
    if (ret != 0)
    {
        fprintf(stderr, "%s: %d Error closing file %s\n", strerror(errno), ret, filename);
        return Py_BuildValue("i", -1);
    }

#ifdef COUNTREP
    gl_err = graphlib_deserializeBasicGraph(&graph_ptr, statCountRepFunctions, buf, (unsigned int)buf_len);
#else
    gl_err = graphlib_deserializeBasicGraph(&graph_ptr, statBitVectorFunctions, buf, (unsigned int)buf_len);
#endif
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "Error serializing graph\n");
        return Py_BuildValue("i", -1);
    }

    if (graphs == NULL)
        graphs = new vector<graphlib_graph_p>;
    graphs->push_back(graph_ptr);
    handle = graphs->size() - 1;

    return Py_BuildValue("i", handle);
}

static PyObject *py_Output_Graph(PyObject *self, PyObject *args)
{
    char *filename;
    int handle;
    graphlib_error_t gl_err;
    graphlib_graph_p graph_ptr = NULL;

    if (!PyArg_ParseTuple(args, "is", &handle, &filename))
    {
        fprintf(stderr, "Failed to parse args, expecting (int, string)\n");
        return Py_BuildValue("i", -1);
    }

    graph_ptr = (*graphs)[handle];

    gl_err = graphlib_colorGraphByLeadingEdgeLabel(graph_ptr);
    if (GRL_IS_FATALERROR(gl_err))
    {
        fprintf(stderr, "graphlib error coloring graph by leading edge label\n");
        return Py_BuildValue("i", -1);
    }

    gl_err =  graphlib_scaleNodeWidth(graph_ptr, 80, 160);
    if ( GRL_IS_FATALERROR(gl_err) )
    {
        fprintf(stderr, "graphlib error scaling node width\n");
        return Py_BuildValue("i", -1);
    }

    gl_err = graphlib_exportGraph(filename, GRF_DOT, graph_ptr);
    if ( GRL_IS_FATALERROR(gl_err) )
    {
        fprintf(stderr, "graphlib error exporting graph\n");
        return Py_BuildValue("i", -1);
    }

    gl_err = graphlib_delGraph(graph_ptr);
    if ( GRL_IS_FATALERROR(gl_err) )
    {
        fprintf(stderr, "graphlib error deleting graph\n");
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", 0);
}

static PyMethodDef _STATmergeMethods[] = {
    {"Init_Graphlib", py_Init_Graphlib, METH_VARARGS, "graphlib init."},
    {"New_Graph", py_New_Graph, METH_VARARGS, "create new graph."},
    {"Add_Trace", py_Add_Trace, METH_VARARGS, "trace generator."},
    {"Merge_Traces", py_Merge_Traces, METH_VARARGS, "trace merger."},
    {"Serialize_Graph", py_Serialize_Graph, METH_VARARGS, "serialize graph."},
    {"Deserialize_Graph", py_Deserialize_Graph, METH_VARARGS, "deserialize graph."},
    {"Output_Graph", py_Output_Graph, METH_VARARGS, "graph exporter."},
    {NULL, NULL, 0, NULL}
};

void init_STATmerge()
{
    Py_InitModule("_STATmerge", _STATmergeMethods);
}

}
