#include <DysectAPI.h>

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
        return Err::verbose(false, "rank negative");
      }
      mpiRankIntervals.insert(pair<mpi_rank_t, mpi_rank_t>(rank, rank));
    } else if(ranks.size() == 2) {
      // Interval
      mpi_rank_t start = atoi(ranks[0].c_str());
      mpi_rank_t end =   atoi(ranks[1].c_str());

      if((start < 0) || (end < 0) || (start > end)) {
        return Err::verbose(false, "Start and end interval not valid: %d -> %d", start, end);
      }

      mpiRankIntervals.insert(pair<mpi_rank_t, mpi_rank_t>(start, end));
    } else {
      return Err::verbose(false, "Ill-formed interval");
    }
  }

  return true;
}

string Group::getExpr()   { return groupExpr; }
