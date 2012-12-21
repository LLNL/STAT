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
#include "STAT_BackEnd.h"


//! The STATBench daemon main
int main(int argc, char **argv)
{
    bool isHelperDaemon = true;
    STAT_BackEnd *statBackEnd;
    StatError_t statError;
    StatDaemonLaunch_t launchType = STATD_LMON_LAUNCH;

    /* If user querying for version, print it and exit */
    if (argc == 2)
    {
        if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)
        {
            printf("STATBenchD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            return 0;
        }
    }

    /* TODO this needs to be changed! We could be sending the log output dir */
    if (argc == 2)
    {
        isHelperDaemon = false;
        launchType = STATD_SERIAL_LAUNCH;
    }

    /* Initialize STAT */
    statError = statInit(&argc, &argv, launchType);
    if (statError != STAT_OK)
    {
        fprintf(stderr, __FILE__, __LINE__, "Failed to initialize STAT\n");
        return statError;
    }

    statBackEnd = new STAT_BackEnd(launchType);

    if (isHelperDaemon == false)
    {
        /* We're the STATBench BE, not the helper daemon */
        /* Connect to MRNet */
        statError = statBackEnd->statBenchConnect();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed to connect BE\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }

        /* Run the main feedback loop */
        statError = statBackEnd->mainLoop();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failure in STAT BE main loop\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }
    }
    else
    {
        /* We're the STATBench helper daemon */
        /* Initialize Launchmon */
        statError = statBackEnd->init();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed to initialize BE\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }

        /* Gather MRNet info and dump to file */
        statError = statBackEnd->statBenchConnectInfoDump();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed to dump connection info\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }
    }

    delete statBackEnd;

    /* Finalize STAT */
    statError = statFinalize(launchType);
    if (statError != STAT_OK)
    {
        fprintf(stderr, "Failed to finalize LMON\n");
        return statError;
    }

    return 0;
}

