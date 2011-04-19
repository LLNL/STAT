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



/*********************************************************
 A benchmark that emulates STAT: the Stack Trace
 Analysis tool.

 Written by: Greg Lee
*********************************************************/


#include "config.h"

#include <getopt.h>
#include "STAT_FrontEnd.h"

using namespace MRN;
using namespace std;

int maxDepth;                           /*!< the max trace depth */
int nTasks;                             /*!< the number of tasks per daemon */
int nTraces;                            /*!< the number of traces per task */
int functionFanout;                     /*!< the max function fanout */
int iters;                              /*!< the number of merge iterations */
int nEqClasses;                         /*!< the number of equivalence classes */
char topologySpecification[BUFSIZE];    /*!< the topology specification */
char *nodeList = NULL;                  /*!< the list of nodes for CPs */
bool shareAppNodes = false;             /*!< whether to use the application nodes to run communication processes */
StatTopology_t topologyType;            /*!< the topology specification type */

//! Prints the usage directions
void STAT_PrintUsage(int argc, char **argv);

//! Parses the command line arguments and sets class variables
StatError_t Parse_Args(STAT_FrontEnd *STAT, int argc, char **argv);

//! The STATBench main
int main(int argc, char **argv)
{
    int i;
    char statBenchDaemon[BUFSIZE], perfData[BUFSIZE];
    STAT_FrontEnd *STAT = new STAT_FrontEnd();
    StatError_t statError;

    /* Set default values */
    maxDepth = 7;
    nTasks = 128;
    nTraces = 3;
    functionFanout = 2;
    iters = 5;
    nEqClasses = -1;
    snprintf(statBenchDaemon, BUFSIZE, "%s/bin/STATBenchD", STAT->getInstallPrefix());
    STAT->setToolDaemonExe(statBenchDaemon);

    /* Parse the arguments */
    statError = Parse_Args(STAT, argc, argv);
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to parse arguments\n");
        return -1;
    }

    /* Push the arguments into the output file */
    snprintf(perfData, BUFSIZE, "STATBench invoked with: ");
    for (i = 0; i < argc; i++)
    {
        strncat(perfData, argv[i], BUFSIZE);
        strncat(perfData, " ", BUFSIZE);
    }
    strncat(perfData, "\n", BUFSIZE);
    STAT->addPerfData(perfData, -1.0);

    /* Print the STATBench header to the output file and to the screen */
    snprintf(perfData, BUFSIZE,
         "#######################################\n"
         "# STATBench: STAT emulation Benchmark #\n"
         "#######################################\n"
         "This benchmark emulates STAT, the Stack Trace\n"
         "Analysis Tool and can determine the expected\n"
         "performance for a specified machine architecture\n"
         "and application profile.\n\n"
         "Running %d iterations with %d tasks, %d traces per task,\n"
         "%d max call depth after main, %d function fanout, and %d\n"
         "equivalence classes.\n\n",
         iters, nTasks, nTraces, maxDepth, functionFanout, nEqClasses);

    STAT->addPerfData(perfData, -1.0);
    printf("%s", perfData);

    /* Launch the MRNet Tree */
    statError = STAT->launchAndSpawnDaemons(NULL, true);
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to launch MRNet tree()\n");
        STAT->shutDown();
        delete STAT;
        return -1;
    }

    /* Launch the MRNet Tree */
    statError = STAT->launchMrnetTree(topologyType, topologySpecification, nodeList, shareAppNodes, true, true);
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to launch MRNet tree()\n");
        STAT->shutDown();
        delete STAT;
        return -1;
    }

    /* Generate the traces */
    statError = STAT->statBenchCreateStackTraces(maxDepth, nTasks, nTraces, functionFanout, nEqClasses);
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to generate stack traces\n");
        STAT->shutDown();
        return -1;
    }

    /* Gather the traces */
    for (i = 0; i < iters; i++)
    {
        statError = STAT->gatherLastTrace();
        if (statError != STAT_OK)
        {
            STAT->printMsg(statError, __FILE__, __LINE__, "Failed to gather stack traces\n");
            STAT->shutDown();
            delete STAT;
            return -1;
        }
    }

    /* Gather the traces */
    for (i = 0; i < iters; i++)
    {
        statError = STAT->gatherTraces();
        if (statError != STAT_OK)
        {
            STAT->printMsg(statError, __FILE__, __LINE__, "Failed to gather stack traces\n");
            STAT->shutDown();
            delete STAT;
            return -1;
        }
    }

    STAT->shutDown();
    printf("\nResults written to %s\n\n", STAT->getOutDir());

    delete STAT;

    return 0;
}

void STAT_PrintUsage(int argc, char **argv)
{
    fprintf(stderr, "\nUsage:\n\t%s [ OPTIONS ]\n\n",  argv[0]);
    fprintf(stderr, "OPTIONS:\n");
    fprintf(stderr, "\nStack trace generation options:\n");
    fprintf(stderr, "  -t, --traces <count>\t\tgenerate <count> traces.\n");
    fprintf(stderr, "  -i, --iters <count>\t\tperform <count> gathers.\n");
    fprintf(stderr, "  -N, --numtasks <count>\temulate <count> tasks per daemon.\n");
    fprintf(stderr, "  -m, --maxdepth <depth>\tgenerate traces with a maximum depth of <depth>.\n");
    fprintf(stderr, "  -b, --branch <width>\t\tgenerate traces with a max branching factor of\n\t\t\t\t<width>.\n");
    fprintf(stderr, "  -e, --eqclasses <count>\tgenerate traces within <count> equivalent\n\t\t\t\tclasses.\n\n");
    fprintf(stderr, "Topology options:\n");
    fprintf(stderr, "  -a, --autotopo\t\tlet STAT automatically create topology\n");
    fprintf(stderr, "  -d, --depth <depth>\t\ttree topology depth\n");
    fprintf(stderr, "  -f, --fanout <width>\t\tmaximum tree topology fanout\n");
    fprintf(stderr, "  -u, --usertopology <topology>\tspecify the number of communication nodes per\n\t\t\t\tlayer in the tree topology, separated by dashes\n");
    fprintf(stderr, "  -n, --nodes <nodelist>\tlist of nodes for communication processes\n");
    fprintf(stderr, "  -A, --appnodes\t\tuse the application nodes for communication processes\n");
    fprintf(stderr, "\t\t\t\tExample node lists:\thost1\n\t\t\t\t\t\t\thost1,host2\n\t\t\t\t\t\t\thost[1,5-7,9]\n");
    fprintf(stderr, "  -p, --procs <processes>\tthe maximum number of communication processes\n\t\t\t\tper node\n");
    fprintf(stderr, "\nMiscellaneous options:\n");
    fprintf(stderr, "  -D, --daemon <path>\t\tthe full path to the STAT daemon\n");
    fprintf(stderr, "  -F, --filter <path>\t\tthe full path to the STAT filter shared object\n");
    fprintf(stderr, "  -l, --log [FE|BE|ALL]\t\tenable debug logging\n");
    fprintf(stderr, "  -L, --logdir <log_directory>\tlogging output directory\n");
    fprintf(stderr, "\n%% man STATBench\n  for more information\n\n");
}

StatError_t Parse_Args(STAT_FrontEnd *STAT, int argc, char **argv)
{
    int i, opt, optionIndex = 0;
    char *logOutDir = NULL;
    StatError_t statError;
    StatLog_t logType;

    struct option longOptions[] =
    {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"verbose", no_argument, 0, 'v'},
        {"autotopo", no_argument, 0, 'a'},
        {"appnodes", no_argument, 0, 'A'}, 
        {"fanout", required_argument, 0, 'f'},
        {"nodes", required_argument, 0, 'n'},
        {"procs", required_argument, 0, 'p'},
        {"traces", required_argument, 0, 't'},
        {"maxdepth", required_argument, 0, 'm'},
        {"branch", required_argument, 0, 'b'},
        {"eqclasses", required_argument, 0, 'e'},
        {"daemon", required_argument, 0, 'D'},
        {"filter", required_argument, 0, 'F'},
        {"log", required_argument, 0, 'l'},
        {"logdir", required_argument, 0, 'L'},
        {"usertopology", required_argument, 0, 'u'},
        {"depth", required_argument, 0, 'd'},
        {"numtasks", required_argument, 0, 'N'},
        {0, 0, 0, 0}
    };

    topologyType = STAT_TOPOLOGY_DEPTH;
    snprintf(topologySpecification, BUFSIZE, "0");

    while (1)
    {
        opt = getopt_long(argc, argv,"hVvaAf:n:p:t:m:b:e:D:F:l:L:u:d:N:", longOptions, &optionIndex);
        if (opt == -1)
            break;
        switch(opt)
        {
        case 'h':
            STAT_PrintUsage(argc, argv);
            exit(0);
            break;
        case 'V':
            printf("STATBench-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            exit(0);
            break;
        case 'v':
            STAT->setVerbose(STAT_VERBOSE_FULL);
            break;
        case 'a':
            topologyType = STAT_TOPOLOGY_AUTO;
            break;
        case 'f':
            topologyType = STAT_TOPOLOGY_FANOUT;
            snprintf(topologySpecification, BUFSIZE, "%s", optarg);
            break;
        case 'n':
            nodeList = strdup(optarg);
            break;
        case 'A':
            shareAppNodes = true;
            break;
        case 'p':
            STAT->setProcsPerNode(atoi(optarg));
            break;
        case 't':
            nTraces = atoi(optarg);
            break;
        case 'm':
            maxDepth = atoi(optarg);
            break;
        case 'b':
            functionFanout = atoi(optarg);
            break;
        case 'e':
            nEqClasses = atoi(optarg);
            break;
        case 'D':
            statError = STAT->setToolDaemonExe(optarg);
            if (statError != STAT_OK)
            {
                STAT->printMsg(statError, __FILE__, __LINE__, "Failed to set tool daemon exe path\n");
                return statError;
            }
            break;
        case 'F':
            statError = STAT->setFilterPath(optarg);
            if (statError != STAT_OK)
            {
                STAT->printMsg(statError, __FILE__, __LINE__, "Failed to set filter path\n");
                return statError;
            }
            break;
        case 'l':
            if (strcmp(optarg, "FE") == 0)
                logType = STAT_LOG_FE;
            else if (strcmp(optarg, "BE") == 0)
                logType = STAT_LOG_BE;
            else if (strcmp(optarg, "ALL") == 0)
                logType = STAT_LOG_ALL;
            else
            {
                STAT->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Log option must equal FE, BE, or ALL, you entered %s\n", optarg);
                return STAT_ARG_ERROR;
            }
            if (logOutDir != NULL)
            {
                statError = STAT->startLog(logType, logOutDir);
                if (statError != STAT_OK)
                {
                    STAT->printMsg(statError, __FILE__, __LINE__, "Failed start logging\n");
                    return statError;
                }
            }
            break;
        case 'L':
            logOutDir = strdup(optarg);
            if (logType != STAT_LOG_NONE)
            {
                statError = STAT->startLog(logType, logOutDir);
                if (statError != STAT_OK)
                {
                    STAT->printMsg(statError, __FILE__, __LINE__, "Failed start logging\n");
                    return statError;
                }
            }
            break;
        case 'u':
            topologyType = STAT_TOPOLOGY_USER;
            snprintf(topologySpecification, BUFSIZE, "%s", optarg);
            break;
        case 'd':
            topologyType = STAT_TOPOLOGY_DEPTH;
            snprintf(topologySpecification, BUFSIZE, "%s", optarg);
            break;
        case 'N':
            nTasks = atoi(optarg);
            break;
        case '?':
            STAT->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            STAT_PrintUsage(argc, argv);
            return STAT_ARG_ERROR;
        default:
            STAT->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            STAT_PrintUsage(argc, argv);
            return STAT_ARG_ERROR;
        };
    }

    if (STAT->getLauncherArgc() == 1)
        STAT->addLauncherArgv("srun");
    STAT->addLauncherArgv(STAT->getToolDaemonExe());
    STAT->addLauncherArgv("--STATBench");

    return STAT_OK;
}

