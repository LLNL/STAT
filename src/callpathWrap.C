#include "callpathWrap.h"

using namespace std;

extern "C" {

int gMpiRank = 0;
int gCallpathWrapIter = 0;
pthread_mutex_t gCallpathWrapWalkMutex;

void depositlightcorewrap_init();
void depositlightcore_wrap_signal_handler(int signum);

void depositlightcorewrap_init()
{
    callpath_wrap_init(0);
    signal(SIGUSR2, depositlightcore_wrap_signal_handler);
}

void depositlightcore_wrap_signal_handler(int signum)
{
    callpath_wrap_walk("core");
}


void callpath_wrap_init(int rank)
{
    static int init = 0;
    if (init != 0)
        return;
    if (rank == -1)
        // This assumes MPI_Init() has been called already
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    gMpiRank = rank;
    gCallpathWrapIter = 0;
    init++;
}

void callpath_wrap_walk(const char *filename)
{
    // we will lock down this whole routine just to be safe with threads
    pthread_mutex_lock(&gCallpathWrapWalkMutex);

    Callpath path;
    CallpathRuntime runtime;
    stringstream ss;
    ofstream file;
    string pathStr;
    size_t index = 0;

    path = runtime.doStackwalk();
    ss.str("");
    ss<<path;
    pathStr = ss.str();
    ss.str("");

    // path format = "module_path(0x1234) : module2_path(0x5678)"
    // we want to change this to: """
    // module_path+0x1234
    // module2_path+0x5678
    // """
    while (1)
    {
        index = pathStr.find("(0x");
        if (index == string::npos)
            break;
        pathStr.replace(index, 3, "+0x");
    }
    while (1)
    {
        index = pathStr.find(")");
        if (index == string::npos)
            break;
        pathStr.replace(index, 1, "");
    }
    while (1)
    {
        index = pathStr.find(": ");
        if (index == string::npos)
            break;
        pathStr.replace(index, 2, "\n");
    }

    ss<<filename;
    string newFilename;
    if (gCallpathWrapIter != 0)
        ss<<"_"<<gCallpathWrapIter;
    ss<<"."<<gMpiRank;
    newFilename = ss.str();
    ss.str("");
    file.open(newFilename.c_str());
    cout<<newFilename<<endl;
    file<<"+++PARALLEL TOOLS CONSORTIUM LIGHTWEIGHT COREFILE FORMAT version 1.0"<<endl;
    file<<"+++LCB 1.0"<<endl;
    pid_t tid = syscall(__NR_gettid);
    file<<"+++ID Rank: "<<gMpiRank<<" PID: "<<getpid()<<" TID: "<<tid<<endl;
    file<<"+++STACK"<<endl;
    file<<"Module+Offset"<<endl;
    file<<pathStr;
    file<<"\n---STACK"<<endl;
    file<<"---ID"<<endl;
    file<<"---LCB"<<endl;
    file.close();
    gCallpathWrapIter++;
    pthread_mutex_unlock(&gCallpathWrapWalkMutex);
}

} //extern "C"
