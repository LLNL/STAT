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

#include <LibDysectAPI.h>
#include <DysectAPI/Err.h>
#include <DysectAPI/Parser.h>
#include <DysectAPI/Group.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

// Look-up tables for domain expression resolving
Group::Group(string groupExpr, long waitTime, bool lblocking) : groupExpr(groupExpr), 
                                                Domain(waitTime, lblocking, GroupDomain) {
}


bool Group::resolveExpr() {
  /*
   * Format parsed:
   * proc_spec   ::= rank | interval
   *              |  proc_spec
   *              |  proc_spec ',' proc_spec
   *
   * rank        ::= DIGIT
   *
   * interval    ::= rank '-' rank
   *
   * 
   *
   */
  vector<string> proc_specs = Parser::tokenize(groupExpr, ',');
  
  for(int i = 0; i < proc_specs.size(); i++) {
    string& proc_spec = proc_specs[i];
    
    vector<string> ranks = Parser::tokenize(proc_spec, '-');

    if(ranks.size() == 1) {
      // Single rank
      mpi_rank_t rank = atoi(ranks[0].c_str());
      if(rank < 0) {
        return DYSECTVERBOSE(false, "rank negative");
      }
      mpiRankIntervals.insert(pair<mpi_rank_t, mpi_rank_t>(rank, rank));
    } else if(ranks.size() == 2) {
      // Interval
      mpi_rank_t start = atoi(ranks[0].c_str());
      mpi_rank_t end =   atoi(ranks[1].c_str());

      if((start < 0) || (end < 0) || (start > end)) {
        return DYSECTVERBOSE(false, "Start and end interval not valid: %d -> %d", start, end);
      }

      mpiRankIntervals.insert(pair<mpi_rank_t, mpi_rank_t>(start, end));
    } else {
      return DYSECTVERBOSE(false, "Ill-formed interval");
    }
  }

  return true;
}

string Group::getExpr()   { return groupExpr; }
