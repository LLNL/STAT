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
#include "STAT.h"
#include "STAT_BackEnd.h"


//! The STATBench daemon main
int main(int argc, char **argv)
{
    STAT_BackEnd *stat_be;
    StatError_t statError;

    /* If user querying for version, print it and exit */
    if (argc == 2)
    {
        if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)
        {
            printf("STATBenchD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            return 0;
        }
    }

    stat_be = new STAT_BackEnd();

    /* TODO this needs to be changed! We could be sending the log output dir */
    if (argc == 2)
    {
        /* We're the STATBench BE, not the helper daemon */
        /* Connect to MRNet */
        statError = stat_be->statBenchConnect();
        if (statError != STAT_OK)
        {
            stat_be->printMsg(statError, __FILE__, __LINE__, "Failed to connect BE\n");
            return -1;
        }

        /* Run the main feedback loop */
        statError = stat_be->mainLoop();
        if (statError != STAT_OK)
        {
            stat_be->printMsg(statError, __FILE__, __LINE__, "Failure in STAT BE main loop\n");
            return -1;
        }
    }
    else
    {
        /* We're the STATBench helper daemon */
        /* Initialize STAT */
        statError = statInit(&argc, &argv);
        if (statError != STAT_OK)
        {
            fprintf(stderr, __FILE__, __LINE__, "Failed to initialize STAT\n");
            return -1;
        }

        /* Initialize Launchmon */
        statError = stat_be->Init();
        if (statError != STAT_OK)
        {
            stat_be->printMsg(statError, __FILE__, __LINE__, "Failed to initialize BE\n");
            return -1;
        }

        /* Gather MRNet info and dump to file */
        statError = stat_be->statBenchConnectInfoDump();
        if (statError != STAT_OK)
        {
            stat_be->printMsg(statError, __FILE__, __LINE__, "Failed to dump connection info\n");
            return -1;
        }

        /* Finalize STAT */
        statError = statFinalize();
        if (statError != STAT_OK)
        {
            fprintf(stderr, "Failed to finalize LMON\n");
            return -1;
        }
    }

    /* Sleep for a second to give MRNet time to exit cleanly */
    sleep(5);

    delete stat_be;
    return 0;
}

