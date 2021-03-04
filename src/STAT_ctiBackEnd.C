#include "STAT_ctiBackEnd.h"

using namespace std;
using namespace MRN;

#include "common_tools_be.h"

STAT_ctiBackEnd::STAT_ctiBackEnd(StatDaemonLaunch_t launchType)
    : STAT_BackEnd(launchType)
{}


StatError_t STAT_ctiBackEnd::init(int *argc, char ***argv)
{
    return STAT_BackEnd::init(argc, argv);
}

StatError_t STAT_ctiBackEnd::finalize()
{
    return STAT_OK;
}

StatError_t STAT_ctiBackEnd::initLmon()
{
    /*
    struct StatBackEndProcInfo_t
    {
    char* executable_name;   // optional
    char* host_name;     // optional
    pid_t pid;
    int mpirank;
    };
    */

    // get the proc table
    cti_pidList_t* pids = cti_be_findAppPids();
    if (!pids)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "CTI failed to get processes.\n");
        return STAT_LMON_ERROR;
    }

    int n = pids->numPids;
    if (n <= 0) {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "CTI found no procesess.\n");
        cti_be_destroyPidList(pids);
        return STAT_LMON_ERROR;
    }

    proctab_ = (StatBackEndProcInfo_t*) malloc(n * sizeof(StatBackEndProcInfo_t*));
    for (int i=0; i<n; ++i) {
        proctab_[i].executable_name = proctab_[i].host_name = nullptr;
        proctab_[i].pid = pids->pids[i].pid;
        proctab_[i].mpirank = pids->pids[i].rank;
    }

    cti_be_destroyPidList(pids);
    
    return STAT_OK;
}

StatError_t STAT_ctiBackEnd::connect(int argc, char **argv)
{
    int i, newProctabSize, oldProctabSize, index;
    unsigned int j, k, nextNodeRank;
    bool found;
    char *param[6], parentPort[BUFSIZE], parentRank[BUFSIZE], myRank[BUFSIZE];
    pid_t pid;
    string prettyHost, leafPrettyHost;
    StatBackEndProcInfo_t *oldProctab, *newProctab;
    StatLeafInfoArray_t leafInfoArray;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Preparing to connect to MRNet\n");

    if (argc == 0 || argv == NULL)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Connecting without backend arguments is not supported in CTI implementation.\n");
        return STAT_LMON_ERROR;
    } /* if (argc == 0 || argv == NULL) */

    /* We were launched by MRNet */
    /* Store my connection information */
    parentHostName_ = strdup(argv[1]);
    if (parentHostName_ == NULL)
    {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to parentHostName_\n", strerror(errno), argv[1]);
            return STAT_ALLOCATE_ERROR;
    }
    parentPort_ = atoi(argv[2]);
    parentRank_ = atoi(argv[3]);
    myRank_ = atoi(argv[5]);

    /* Connect to the MRNet Network */
    printMsg(STAT_LOG_MESSAGE, __FILE__ ,__LINE__, "Calling CreateNetworkBE with %d args:\n", argc);
    for (i = 0; i < argc; i++)
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "\targv[%d] = %s\n", i, argv[i]);

    network_ = Network::CreateNetworkBE(6, argv);
    if (network_ == NULL)
    {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "CreateNetworkBE() failed\n");
            return STAT_MRNET_ERROR;
   }
    printMsg(STAT_LOG_MESSAGE,__FILE__,__LINE__,"CreateNetworkBE successfully completed\n");

    connected_ = true;

    return STAT_OK;
}

/******************
 * STATBench Code *
 ******************/

StatError_t STAT_ctiBackEnd::statBenchConnectInfoDump()
{
    printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Statbench info dump not supported in CTI implementatioin.\n");
    return STAT_SYSTEM_ERROR;
}

STAT_BackEnd* STAT_BackEnd::make(StatDaemonLaunch_t launchType)
{
    return new STAT_ctiBackEnd(launchType);
}
