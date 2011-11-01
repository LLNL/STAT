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


#include "config.h"

#include <getopt.h>
#include "STAT_FrontEnd.h"

using namespace std;
using namespace MRN;

//! Prints the usage directions
void printUsage(int argc, char **argv);

//! Parses the command line arguments and sets the STAT class variables
/*!
    \param STAT - the STAT Frontend object
    \param argc - the number of arguments
    \param argv - the arguments
    \return STAT_OK on success
*/
StatError_t parseArgs(STAT_FrontEnd *STAT, int argc, char **argv);

//! Sleeps for requested amount of time and prints a message to inform the user
void mySleep();

int sleepTime;                          /*!< the sleep duration */
unsigned int pid;                       /*!< the job launcher PID */
unsigned int nTraces;                   /*!< the number of traces to gather*/
unsigned int traceFrequency;            /*!< the time between samples */
unsigned int nRetries;                  /*!< the number of retries per sample */
unsigned int retryFrequency;            /*!< the time between retries */
char topologySpecification[BUFSIZE];    /*!< the topology specification */
char *remoteNode = NULL;                /*!< the remote node running the job launcher */
char *nodeList = NULL;                  /*!< the CP node list */
bool individualSamples = false;         /*!< whether to gather individual samples when sampling multiple */
bool comprehensive = false;             /*!< whether to gather a comprehensive set of samples */
bool withThreads = false;               /*!< whether to gather samples from helper threads */
bool clearOnSample = true;              /*!< whether to clear accumulated traces when sampling */
bool shareAppNodes = false;             /*!< whether to use the application nodes to run communication processes */
StatLaunch_t applicationOption;         /*!< attach or launch case */
StatSample_t sampleType;                /*!< the sample level of detail */
StatTopology_t topologyType;            /*!< the topology specification type */

//! main() function to run STAT
int main(int argc, char **argv)
{
    int i, j, samples = 1, traces;
    STAT_FrontEnd *STAT;
    StatError_t statError, retval;
    string invocationString; 

    /* Parse arguments and fill in class variables */
    STAT = new STAT_FrontEnd();
    statError = parseArgs(STAT, argc, argv);
    if (statError != STAT_OK)
    {
        if (statError != STAT_WARNING)
            STAT->printMsg(statError, __FILE__, __LINE__, "Failed to parse arguments\n");
        delete STAT;
        if (statError != STAT_WARNING)
            return -1;
        else
            return 0;
    }

    /* Push the arguments into the output file */
    invocationString = "STAT invoked with: ";
    for (i = 0; i < argc; i++)
    {
        invocationString.append(argv[i]);
        invocationString.append(" ");
    }
    invocationString.append("\n");
    STAT->addPerfData(invocationString.c_str(), -1.0);

    /* If we're just attaching, sleep here */
    if (applicationOption == STAT_ATTACH)
        mySleep();

    /* Launch the Daemons */
    if (applicationOption == STAT_ATTACH)
        statError = STAT->attachAndSpawnDaemons(pid, remoteNode);
    else
        statError = STAT->launchAndSpawnDaemons(remoteNode);
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to launch MRNet tree()\n");
        STAT->shutDown();
        delete STAT;
        return -1;
    }

    /* Launch the MRNet Tree */
    statError = STAT->launchMrnetTree(topologyType, topologySpecification, nodeList, true, shareAppNodes);
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to launch MRNet tree()\n");
        STAT->shutDown();
        delete STAT;
        return -1;
    }

    /* Attach to the application */
    statError = STAT->attachApplication();
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to attach to application\n");
        STAT->shutDown();
        delete STAT;
        return -1;
    }

    /* If we're launching, sleep here to let the job start */
    if (applicationOption == STAT_LAUNCH)
    {
        statError = STAT->resume();
        if (statError != STAT_OK)
        {
            STAT->printMsg(statError, __FILE__, __LINE__, "Failed to resume application\n");
            STAT->shutDown();
            delete STAT;
            return -1;
        }
        mySleep();
    }

#ifdef SBRS
    sleep(1);
#endif

    clearOnSample = true;
    retval = STAT_OK;
    if (nTraces <= 0)
        traces = 1;
    else
        traces = nTraces;
        
    if (comprehensive)
        samples = 4;
    for (i = 0; i < samples; i++)
    {
        if (comprehensive)
        {
            if (i == 0)
            {
                traces = 1;
                sampleType = STAT_FUNCTION_NAME_ONLY;
            }
            else if (i == 1)
            {
                traces = 1;
                sampleType = STAT_FUNCTION_AND_LINE;
            }
            else if (i == 2)
            {
                traces = 1;
                sampleType = STAT_FUNCTION_AND_PC;
            }
            else
            {
                traces = nTraces;
                sampleType = STAT_FUNCTION_NAME_ONLY;
            }
        }
        for (j = 0; j < traces; j++)
        {
            if (j == 1)
                clearOnSample = false;
            /* Pause the application */
            statError = STAT->pause();
            if (statError == STAT_APPLICATION_EXITED)
                retval = STAT_APPLICATION_EXITED;
            else if (statError != STAT_OK)
            {
                STAT->printMsg(statError, __FILE__, __LINE__, "Failed to resume application\n");
                STAT->shutDown();
                delete STAT;
                return -1;
            }
    
            /* Sample the traces */
            statError = STAT->sampleStackTraces(sampleType, withThreads, clearOnSample, 1, traceFrequency, nRetries, retryFrequency);
            if (statError != STAT_OK)
            {
                if (statError == STAT_APPLICATION_EXITED)
                    break;
                STAT->printMsg(statError, __FILE__, __LINE__, "Failed to sample stack traces\n");
                STAT->shutDown();
                delete STAT;
                return -1;
            }
    
            if (individualSamples == true)
            {
                /* Gather the trace */
                statError = STAT->gatherLastTrace();
                if (statError != STAT_OK)
                {
                    STAT->printMsg(statError, __FILE__, __LINE__, "Failed to gather stack traces\n");
                    STAT->shutDown();
                    delete STAT;
                    return -1;
                }
            }
    
            /* Resume the application */
            if (comprehensive == false or i == 3)
            {
                statError = STAT->resume();
                if (statError == STAT_APPLICATION_EXITED)
                    retval = STAT_APPLICATION_EXITED;
                else if (statError != STAT_OK)
                {
                    STAT->printMsg(statError, __FILE__, __LINE__, "Failed to resume application\n");
                    STAT->shutDown();
                    delete STAT;
                    return -1;
                }
                if (retval != STAT_OK)
                    break;
                if (j != traces - 1)
                    usleep(1000*traceFrequency);
            }
        } /* for j */

        /* Gather the traces */
        if (traces > 1)
            statError = STAT->gatherTraces();
        else
            statError = STAT->gatherLastTrace();
        if (statError != STAT_OK)
        {
            STAT->printMsg(statError, __FILE__, __LINE__, "Failed to gather stack traces\n");
            STAT->shutDown();
            delete STAT;
            return -1;
        }
    } /* for i */

    /* Detach from the application */
    statError = STAT->detachApplication();
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to detach from application\n");
        STAT->shutDown();
        delete STAT;
        return -1;
    }

    STAT->shutDown();
    printf("\nResults written to %s\n\n", STAT->getOutDir());

    delete STAT;
    return 0;
}

void printUsage(int argc, char **argv)
{
    fprintf(stderr, "\nSTAT, the Stack Trace Analysis Tool, is a highly scalable, lightweight tool that gathers and merges stack traces from a parallel application's processes.  STAT can gather and merge multiple stack traces per process, showing the application's time varying behavior.  The output shows the global application behavior and can be used to identify process equivalence classes, subsets of tasks that exhibit similar behavior.  A representative of each equivalence class can be fed into a heavyweight debugger for root cause analysis.  Running STAT will create a STAT_results directory that contains \".dot\" files that are best viewed with the STATview GUI.  They can also be viewed with the \"dotty\" utility from graphviz.\n");
    fprintf(stderr, "\nUsage:\n");
    fprintf(stderr, "\tSTAT [OPTIONS] <launcherPid_>\n");
    fprintf(stderr, "\tSTAT [OPTIONS] -C <application_launch_command>\n");
    fprintf(stderr, "\nStack trace sampling options:\n");
    fprintf(stderr, "  -t, --traces <count>\t\tnumber of traces per process\n");
    fprintf(stderr, "  -T, --tracefreq <frequency>\ttime between samples in milli-seconds\n");
    fprintf(stderr, "  -S, --sampleindividual\tsave all individual samples\n");
    fprintf(stderr, "  -r, --retries <count>\t\tretry attempts per sample to get a complete\n\t\t\t\ttrace\n");
    fprintf(stderr, "  -R, --retryfreq <frequency>\ttime between sample retries\n");
    fprintf(stderr, "  -P, --withpc\t\t\tsample program counter in addition to\n\t\t\t\tfunction name\n");
    fprintf(stderr, "  -i, --withline\t\t\tsample source line number in addition\n\t\t\t\tto function name\n");
    fprintf(stderr, "  -c, --comprehensive\t\tgather 4 traces: function only; function + line;\n\t\t\t\tfunction + pc; and 3D function only\n");
    fprintf(stderr, "  -w, --withthreads\t\tsample helper threads in addition to the\n\t\t\t\tmain thread\n");
    fprintf(stderr, "  -s, --sleep <time>\t\tsleep time before attaching and gathering traces\n");
    fprintf(stderr, "\nTopology options:\n");
    fprintf(stderr, "  -a, --autotopo\t\tlet STAT automatically create topology\n");
    fprintf(stderr, "  -d, --depth <depth>\t\ttree topology depth\n");
    fprintf(stderr, "  -f, --fanout <width>\t\tmaximum tree topology fanout\n");
    fprintf(stderr, "  -u, --usertopology <topology>\tspecify the number of communication nodes per\n\t\t\t\tlayer in the tree topology, separated by dashes\n");
    fprintf(stderr, "  -n, --nodes <nodelist>\tlist of nodes for communication processes\n");
    fprintf(stderr, "  -A, --appnodes\t\tuse the application nodes for communication processes\n");
    fprintf(stderr, "\t\t\t\tExample node lists:\thost1\n\t\t\t\t\t\t\thost1,host2\n\t\t\t\t\t\t\thost[1,5-7,9]\n");
    fprintf(stderr, "  -p, --procs <processes>\tthe maximum number of communication processes\n\t\t\t\tper node\n");
    fprintf(stderr, "\nMiscellaneous options:\n");
    fprintf(stderr, "  -C, --create [<args>]+\tlaunch the application under STAT's control.\n\t\t\t\tAll args after -C are used to launch the app.\n");
    fprintf(stderr, "  -D, --daemon <path>\t\tthe full path to the STAT daemon\n");
    fprintf(stderr, "  -F, --filter <path>\t\tthe full path to the STAT filter shared object\n");
    fprintf(stderr, "  -l, --log [FE|BE|ALL]\t\tenable debug logging\n");
    fprintf(stderr, "  -L, --logdir <log_directory>\tlogging output directory\n");
    fprintf(stderr, "  -j, --jobid <id>\t\tappend <id> to the STAT output directory\n");
    fprintf(stderr, "\n%% man STAT\n  for more information\n\n");
}

StatError_t parseArgs(STAT_FrontEnd *STAT, int argc, char **argv)
{
    int i, opt, optionIndex = 0;
    string remotePid, remoteHost;
    string::size_type colonPos;
    StatError_t statError;
    string nProcsOpt;
    int nProcs;
    bool createJob = false;
    char *logOutDir = NULL;
    StatLog_t logType = STAT_LOG_NONE;

    struct option longOptions[] =
    {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"verbose", no_argument, 0, 'v'},
        {"withpc", no_argument, 0, 'P'},
        {"withline", no_argument, 0, 'i'},
        {"comprehensive", no_argument, 0, 'c'},
        {"withthreads", no_argument, 0, 'w'},
        {"autotopo", no_argument, 0, 'a'},
        {"create", no_argument, 0, 'C'}, 
        {"appnodes", no_argument, 0, 'A'}, 
        {"sampleindividual", no_argument, 0, 'S'}, 
        {"fanout", required_argument, 0, 'f'},
        {"nodes", required_argument, 0, 'n'},
        {"procs", required_argument, 0, 'p'},
        {"jobid", required_argument, 0, 'j'},
        {"retries", required_argument, 0, 'r'},
        {"retryfreq", required_argument, 0, 'R'},
        {"traces", required_argument, 0, 't'},
        {"tracefreq", required_argument, 0, 'T'},
        {"daemon", required_argument, 0, 'D'},
        {"filter", required_argument, 0, 'F'},
        {"sleep", required_argument, 0, 's'},
        {"log", required_argument, 0, 'l'},
        {"logdir", required_argument, 0, 'L'},
        {"usertopology", required_argument, 0, 'u'},
        {"depth", required_argument, 0, 'd'},
        {0, 0, 0, 0}
    };

    sleepTime = -1;
    nTraces = 10;
    traceFrequency = 100;
    nRetries = 0;
    retryFrequency = 100;
    sampleType = STAT_FUNCTION_NAME_ONLY;
    topologyType = STAT_TOPOLOGY_AUTO;
    snprintf(topologySpecification, BUFSIZE, "0");

    while (1)
    {
        opt = getopt_long(argc, argv,"hVvPicwaCSAf:n:p:j:r:R:t:T:d:F:s:l:L:u:D:", longOptions, &optionIndex);
        if (opt == -1)
            break;
        if (opt == 'C')
        {
            createJob = true;
            break; /* All remaining args are for job launch */
        }
        switch(opt)
        {
        case 'h':
            printUsage(argc, argv);
            return STAT_WARNING;
            break;
        case 'V':
            printf("STAT-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            return STAT_WARNING;
            break;
        case 'v':
            STAT->setVerbose(STAT_VERBOSE_FULL);
            break;
        case 'w':
            withThreads = true;
            break;
        case 'P':
            sampleType = STAT_FUNCTION_AND_PC;
            break;
        case 'i':
            sampleType = STAT_FUNCTION_AND_LINE;
            break;
        case 'c':
            comprehensive = true;
            break;
        case 'a':
            topologyType = STAT_TOPOLOGY_AUTO;
            break;
        case 'd':
            topologyType = STAT_TOPOLOGY_DEPTH;
            snprintf(topologySpecification, BUFSIZE, "%s", optarg);
            break;
        case 'f':
            topologyType = STAT_TOPOLOGY_FANOUT;
            snprintf(topologySpecification, BUFSIZE, "%s", optarg);
            break;
        case 'u':
            topologyType = STAT_TOPOLOGY_USER;
            snprintf(topologySpecification, BUFSIZE, "%s", optarg);
            break;
        case 'S':
            individualSamples = true;
            break;
        case 'A':
            shareAppNodes = true;
            break;
        case 'n':
            nodeList = strdup(optarg);
            break;
        case 'p':
            STAT->setProcsPerNode(atoi(optarg));
            break;
        case 'j':
            STAT->setJobId(atoi(optarg));
            break;
        case 'r':
            nRetries = atoi(optarg);
            break;
        case 'R':
            retryFrequency = atoi(optarg);
            break;
        case 't':
            nTraces = atoi(optarg);
            break;
        case 'T':
            traceFrequency = atoi(optarg);
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
        case 's':
            sleepTime = atoi(optarg);
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
        case '?':
            STAT->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            printUsage(argc, argv);
            return STAT_ARG_ERROR;
        default:
            STAT->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            printUsage(argc, argv);
            return STAT_ARG_ERROR;
        };
    }

    if (optind == argc - 1 && createJob == false)
    {
        applicationOption = STAT_ATTACH;
        remotePid = argv[optind++];
        colonPos = remotePid.find_first_of(":");
        if (colonPos != string::npos)
        {
            remoteHost = remotePid.substr(0, colonPos);
            remoteNode = strdup(remoteHost.c_str());
            if (remoteNode == NULL)
            {
                STAT->printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to set remote node from %s\n", remotePid.c_str());
                return STAT_ALLOCATE_ERROR;
            }
            pid = atoi((remotePid.substr(colonPos + 1, remotePid.length())).c_str());
        }
        else
        {
            pid = atoi(remotePid.c_str());
        }
    }
    else if (optind < argc - 1 && createJob == true)
    {
        applicationOption = STAT_LAUNCH;
        for (i = optind; i < argc; i++)
            STAT->addLauncherArgv(argv[i]);
    }
    else
    {
        printUsage(argc, argv);
        return STAT_ARG_ERROR;
    }

    return STAT_OK;
}

void mySleep()
{
    if (sleepTime > 0)
    {
        time_t currentTime;
        struct tm *localTime;
        currentTime = time(NULL);
        localTime = localtime(&currentTime);
        if (localTime == NULL)
            printf("STAT Sleeping for %d seconds... ", sleepTime);
        else
            printf("%s STAT Sleeping for %d seconds... ", asctime(localTime), sleepTime);
        fflush(stdout);
        sleep(sleepTime);
        printf("STAT has awoken\n");
        fflush(stdout);
    }
}

