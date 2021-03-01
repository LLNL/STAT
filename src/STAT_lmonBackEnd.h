#ifndef __STAT_LMONBACKEND_H
#define __STAT_LMONBACKEND_H

#include "STAT_BackEnd.h"

#include "lmon_api/lmon_be.h"

class STAT_lmonBackEnd : public STAT_BackEnd
{
    public:

    STAT_lmonBackEnd(StatDaemonLaunch_t launchType);

    virtual StatError_t init(int *argc, char ***argv);
    virtual StatError_t finalize();
    
    //! Get the process table
    /*!
      \return the process table
    */
    MPIR_PROCDESC_EXT *getProctab();

    //! Initialize and set up LaunchMON
    /*
      \return STAT_OK on success
    */
    virtual StatError_t initLmon();

    virtual StatError_t connect(int argc = 0, char **argv = NULL);
    virtual StatError_t statBenchConnectInfoDump();
};


#endif
