/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __GROUP_H
#define __GROUP_H

namespace DysectAPI {
  typedef long mrnet_rank_t;
  typedef long mpi_rank_t;
  typedef long seq_t;
  
  class Group : public Domain {
  friend class Probe;

    static bool generateSequenceMap();

    static std::map<mpi_rank_t, seq_t>         mpiRankToSequenceMap;
    static std::map<mpi_rank_t, mrnet_rank_t>  mpiRankToMrnetRankMap;
    static std::map<seq_t, mrnet_rank_t>       sequenceToMrnetRankMap;
  
    Dyninst::ProcControlAPI::ProcessSet::ptr   procSubset;
    
    bool addMpiRank(mpi_rank_t rank); //!< Adds MRNet ranks to group based on MPI rank

    /** Adds range of MRNet ranks based on MPI rank interval.
        generateSequenceMap() creates maps needed for this lookup.
        Among others, MPI ranks are sorted and given sequence numbers
        based on connected MRNet rank (daemon).
       
        Example:
          mpi rank:   deamon rank:    Sequence generated:
                 0              0            0
                 1              0            0
                 2              1            1
                 3              0            2
                 4              1            3
       
        A new sequence is generated when daemon changes (cur != prev)
        in interval.
        A mpi rank interval will then be resolved by collecting 
        sequence numbers between start and end and adding corresponding
        daemon ranks.
       
        Example:
       
        MPI rank interval (1 - 3)
        
        mpiToSeq[1] = 0
        mpiToSeq[3] = 2
       
        Daemons = seqToDaemon[0..2];
    */
    bool addMpiRankRange(mpi_rank_t start, mpi_rank_t end);

    /** MRNet ranks contains all daemon ranks
        connected to desired subset of MPI ranks.
        Daemon ranks can and will probably span wider
        than desired MPI rank subset - further filtering is therefore
        needed when performing actions on group */
    std::set<MRN::Rank> mrnetRanks;
    std::set<std::pair<mpi_rank_t, mpi_rank_t> > mpiRankIntervals;

    std::string groupExpr;

    bool resolveExpr();
    bool getMRNetRanksFromIntervals();

    bool daemonRanksStr(std::string& str);
    
    bool lookupAttached();

    std::set<mpi_rank_t> subsetMPIRanks;

  public:
    Group(std::string groupExpr, long waitTime = Wait::inf, bool lblocking = false);

    bool anyTargetsAttached();

    bool getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset);

    DysectErrorCode createStream();
    DysectErrorCode prepareStream();

    std::string getExpr();
  };
}
#endif
