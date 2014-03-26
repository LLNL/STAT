#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

//
// Group class statics
//

map<mpi_rank_t, seq_t>        Group::mpiRankToSequenceMap;
map<mpi_rank_t, mrnet_rank_t> Group::mpiRankToMrnetRankMap;
map<seq_t, mrnet_rank_t>      Group::sequenceToMrnetRankMap;


bool Group::generateSequenceMap() {
  if((!processTable) || (processTableSize <= 0) || (!mrnetRankToMpiRanksMap)) {
    return Err::warn(false, "Process table not available for group expression lookup");
  }

  sequenceToMrnetRankMap.clear();
  mpiRankToMrnetRankMap.clear();
  mpiRankToSequenceMap.clear();

  // Create sorted list with MPI ranks
  std::list<mpi_rank_t> mpiRanks;

  map<int, IntList_t*>::iterator iter;
  iter = mrnetRankToMpiRanksMap->begin();
  for(; iter != mrnetRankToMpiRanksMap->end(); iter++) {
    mrnet_rank_t mrnetRank = iter->first;
    IntList_t* ranks = iter->second;

    assert(ranks != 0);

    for(int i = 0; i < ranks->count; i++) {
       mpi_rank_t mpiRank = ranks->list[i];

       mpiRanks.push_back(mpiRank);
       mpiRankToMrnetRankMap.insert(pair<mpi_rank_t, mrnet_rank_t>(mpiRank, mrnetRank));
    }
  }
  mpiRanks.sort();

  // Create sequence numbers for ranks
  // and save MRNet rank for generated sequence numbers
  int prevMrnetRank = -1;
  int sequenceNumber = 0;

  list<long>::iterator lIter = mpiRanks.begin();
  for(;lIter != mpiRanks.end(); lIter++) {
    mpi_rank_t mpiRank = *(lIter);
    
    mpi_rank_t curMrnetRank = mpiRankToMrnetRankMap[mpiRank];
    if(prevMrnetRank != curMrnetRank) {
      sequenceNumber++;
    }

    mpiRankToSequenceMap.insert(pair<mpi_rank_t, seq_t>(mpiRank, sequenceNumber));
    sequenceToMrnetRankMap.insert(pair<seq_t, mrnet_rank_t>(sequenceNumber, curMrnetRank));

    prevMrnetRank = curMrnetRank;
  }

  // mpi rank start -> sequence start [x]     \
  //                                          |-> Sequences in between -> MRNet rank members of interval 
  // mpi rank end   -> sequence end   [x + N] /
  
  return true;
}

bool Group::addMpiRank(mpi_rank_t rank) {
  // Rank present?
  if(mpiRankToMrnetRankMap.find(rank) == mpiRankToMrnetRankMap.end()) {
    return Err::warn(false, "MPI Rank '%d' not present in current session", rank); 
  }

  mrnet_rank_t mrnetRank = mpiRankToMrnetRankMap[rank]; 

  mrnetRanks.insert(mrnetRank);

  return true;
}

bool Group::addMpiRankRange(mpi_rank_t start, mpi_rank_t end) {
  if((mpiRankToMrnetRankMap.find(start) == mpiRankToMrnetRankMap.end()) ||
     (mpiRankToMrnetRankMap.find(end)   == mpiRankToMrnetRankMap.end())) {
       
    return Err::warn(false, "MPI start or end rank of interval '%d -> %d' not present in current session", start, end); 
  }

  seq_t startSeq = mpiRankToSequenceMap[start];
  seq_t endSeq   = mpiRankToSequenceMap[end];

  for(int i = startSeq; i <= endSeq; i++) {
    mrnet_rank_t mrnetRank = sequenceToMrnetRankMap[i];
    mrnetRanks.insert(mrnetRank);
  }

  return true;
}

bool Group::getMRNetRanksFromIntervals() {
  set<pair<mpi_rank_t, mpi_rank_t> >::iterator intervalIter;
  pair<mpi_rank_t, mpi_rank_t> interval;
  int start, end;

  for(intervalIter = mpiRankIntervals.begin(); intervalIter != mpiRankIntervals.end(); intervalIter++) {
    interval = *intervalIter;

    start = interval.first;
    end = interval.second;

    if(start == end) {
      if(!addMpiRank(start)) {
        return Err::verbose(false, "addMpiRank failed");
      }

    } else {
      if(!addMpiRankRange(start, end)) {
        return Err::verbose(false, "addMpiRange failed");
      }

    }
  }

  string daemonRanks;
  daemonRanksStr(daemonRanks);

  Err::info(true, "Expression: '%s' has been resolved to daemon ranks: %s", groupExpr.c_str(), daemonRanks.c_str());

  return true;
}

bool Group::daemonRanksStr(string& str) {
  if(mrnetRanks.empty()) {
    str = "none";
  } else {
    set<MRN::Rank>::iterator rankIter;
    const int bufSize = 32;
    char buf[bufSize];
    int rank;

    for(rankIter = mrnetRanks.begin(); rankIter != mrnetRanks.end(); rankIter++) {
      rank = *rankIter;
      snprintf((char*)&buf, bufSize, "%d,", rank);
      str.append((char*)buf);
    }
  }

  return true;
}

bool Group::anyTargetsAttached() {
  assert(!"Frontend can not fetch targets attached to domain. Use backends.");
  return false;
}

bool Group::getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset) {
  assert(!"Frontend can not fetch targets attached to domain. Use backends.");
  return false;
}

DysectErrorCode Group::createStream() {
  assert(network != 0);

  if(Group::mpiRankToSequenceMap.empty()) {
    Group::generateSequenceMap();
  }

  if(!resolveExpr()) {
    return Err::warn(DomainExpressionError, "Failed resolving group expression: \"%s\"", groupExpr.c_str());
  }

  if(!getMRNetRanksFromIntervals()) {
    return Err::warn(DomainExpressionError, "Failed to retrieve MRNet ranks from expression: \"%s\"", groupExpr.c_str());
  }

  // Create stream with daemons and specified synchronization
  comm = network->new_Communicator(mrnetRanks);

  if(!comm) {
    return Err::warn(Error, "MRNet Communicator could not be created from expression");
  }

  return createStreamGeneric();
}

DysectErrorCode Group::prepareStream() {
  return OK;
}
