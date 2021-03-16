#include "STAT_ctiBackEnd.h"
#include "common_tools_be.h"
#include <fstream>
#include <sstream>

STAT_ctiBackEnd::STAT_ctiBackEnd(StatDaemonLaunch_t launchType)
    : STAT_BackEnd(launchType)
{}


StatError_t STAT_ctiBackEnd::init(int *argc, char ***argv)
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Initializing stat-cti backend\n");
    return STAT_BackEnd::init(argc, argv);
}

StatError_t STAT_ctiBackEnd::finalize()
{
    return STAT_OK;
}

StatError_t STAT_ctiBackEnd::initLmon()
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Getting app processes.\n");

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

    proctab_ = (StatBackEndProcInfo_t*) malloc(n * sizeof(StatBackEndProcInfo_t));
    for (int i=0; i<n; ++i) {
        auto pid = pids->pids[i].pid;

        char exe[PATH_MAX+1];
        std::string procpath = "/proc/" + std::to_string(pid) + "/exe";
        ssize_t plen = readlink(procpath.c_str(), exe, PATH_MAX+1);
        if (plen <= 0 || plen == PATH_MAX+1) {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "could not get application executable");
            cti_be_destroyPidList(pids);
            return STAT_LMON_ERROR;
        }
        
        proctab_[i].executable_name = strdup(exe);
        proctab_[i].host_name = nullptr;
        proctab_[i].pid = pid;
        proctab_[i].mpirank = pids->pids[i].rank;
    }
    proctabSize_ = n;

    cti_be_destroyPidList(pids);
    
    return STAT_OK;
}

#if 0
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
#endif

StatError_t STAT_ctiBackEnd::connect(int argc, char **argv)
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "connecting backend XXX\n");
    if (argc != 0) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "not expecting arguments\n");
        return STAT_SYSTEM_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "getting the fileDir\n");
    char* fileDir = cti_be_getFileDir();
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "file dir is %s\n", fileDir);
    
    if (!fileDir) {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "CTI failed to file dir.\n");
        return STAT_LMON_ERROR;
    }
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "jenkies shaggy\n");

    //std::string connectionFile(fileDir);

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "greasy grimy gopher guts \n");

    std::ostringstream os;
    os << fileDir << "/daemoninfo.txt";

    //printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "connection dir is %s\n", connectionFile.c_str());

    
    //std::string connectionFile = std::string(fileDir) + "/daemoninfo.txt";
    //std::string cfilename("daemoninfo.txt");

    //printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "cfilename is %s\n", cfilename.c_str());

    //    connectionFile += cfilename;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "waffles forthwith\n");

    std::string connectionFile = os.str();

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "trying to read file %s\n", connectionFile.c_str());

    free(fileDir);
    fileDir = nullptr;

    bool gotConnectionFile = false;
    {
        std::ifstream connections;
        static const int timeout = 30;  // seconds
        for (int attempt = 0; attempt < 100 * timeout; ++attempt) {
            connections.open(connectionFile.c_str());
            if (connections) {
                gotConnectionFile = true;
                break;
            }
            usleep(10000);
        }
    }
        

    if (gotConnectionFile) {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "successfully opened connection file %s\n",
                 connectionFile.c_str());

        /*
        std::string line;
        while (std::getline(connections, line)) {
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "connection line %s\n", line.c_str());
        }
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "EOF\n");
        connections.close();
        connections.open(connectionFile.c_str());
        */
            
    } else {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "could not open connection file %s\n",
                 connectionFile.c_str());
        return STAT_SYSTEM_ERROR;
    }

    // find the hostname in the list
    char* hostnamePtr = cti_be_getNodeHostname();
    if (!hostnamePtr) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "could not get hostname\n");
        return STAT_SYSTEM_ERROR;
    }

    std::string hostname(hostnamePtr);
    free(hostnamePtr);

    if (FILE *cf = fopen(connectionFile.c_str(), "r")) {
        fseek(cf, 0L, SEEK_END);
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "connection file is %d bytes\n", int(ftell(cf)));
        fclose(cf);
    } else {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "could not fopen connection file\n");
    }

    std::ifstream connections(connectionFile.c_str());
    if (!connections) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "could not reopen connection file %s\n",
                 connectionFile.c_str());
        return STAT_SYSTEM_ERROR;
    }

    int numHosts;
    if (connections >> numHosts) {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "got %d hosts\n", numHosts);
    } else {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "reading number of hosts failed\n");
    }

    if (!connections || numHosts <= 0) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "could not get find parent connection info\n");
        return STAT_SYSTEM_ERROR;
    }

    int parentIdx = -1, mrnRank = -1;
    for (int hostIdx=0; connections && hostIdx < numHosts; ++hostIdx) {
        std::string host; int rank, pidx;
        connections >> host >> rank >> pidx;
        if (parentIdx < 0 && host == hostname) {
            parentIdx = pidx;
            mrnRank = rank;
        }
    }

    if (parentIdx < 0) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "could not find host %s in host list\n",
                 hostname.c_str());
        return STAT_SYSTEM_ERROR;
    }

    int numParents = 0;
    connections >> numParents;
    std::string parentHostname; int parentPort = -1, parentRank = -1;
    std::string phost; int pport, prank;
    for (int idx=0; connections && idx<numParents; ++idx) {
        if (!(connections >> phost >> pport >> prank))
            break;
        
        if (idx == parentIdx) {
            parentHostname = phost;
            parentPort = pport;
            parentRank = prank;
            break;
        }
    }

    if (parentHostname.empty()) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "could not find parent info\n");
        return STAT_SYSTEM_ERROR;
    }

    std::array<std::string,6> sbeArgs = { "",
                                          parentHostname,
                                          std::to_string(parentPort),
                                          std::to_string(parentRank),
                                          hostname,
                                          std::to_string(mrnRank) };
    char* beArgs[6];
    for (int i=0; i<6; ++i) {
        beArgs[i] = const_cast<char*>(sbeArgs[i].c_str());
    }

    for (int i=0; i<6; ++i) {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "mrnet be arg[%d] = %s\n", i, beArgs[i]);
    }
                                           
    network_ = MRN::Network::CreateNetworkBE(6, beArgs);

    if (!network_) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "back end network creation failed\n");
        return STAT_SYSTEM_ERROR;
    }

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
    if (FILE* f = fopen("/home/users/jvogt/tests/stat/log/bex", "a")) {
        fprintf(f, "starting back end\n");
        fclose(f);
    }

    return new STAT_ctiBackEnd(launchType);
}
