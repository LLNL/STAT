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
#include <DysectAPI/Condition.h>
#include <DysectAPI/Domain.h>
#include <DysectAPI/Expr.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;


int Synthetic::filterIdCounter = 0;

string Cond::str() {
  string returnString = "Condition";
  Data *data = NULL;
  Synthetic *synthetic = NULL;

  switch (conditionType)
  {
    case DataCondition:
      returnString = "Data(";
      data = dynamic_cast<Data*>(this);
      if (data != NULL)
        returnString += data->getExpr();
      returnString += ")";
      break;
    case SyntheticCondition:
      returnString = "Synthetic(";
      synthetic = dynamic_cast<Synthetic*>(this);
      if (synthetic != NULL)
        returnString += data->getExpr();
      returnString += ")";
      break;
    default:
      returnString = "Unknown()";
      break;
  }
  return returnString;
}

Cond* Data::eval(string expr) {
 Data* dobj = new Data(expr);

 Cond* cond = dynamic_cast<Cond*>(dobj);
 return cond;
}

Cond* Synthetic::prune(double ratio) {
 DYSECTVERBOSE(true, "Create synthetic prune filter");
 Synthetic* sobj = new Synthetic(ratio);

 sobj->filterId = filterIdCounter++;

 Cond* cond = dynamic_cast<Cond*>(sobj);
 return cond;
}

Cond::Cond(Data *dataExpr) : conditionType(DataCondition), dataExpr(dataExpr) {
}

Cond::Cond() : conditionType(UnknownCondition), dataExpr(0) {
}

Cond::Cond(ConditionType type) : conditionType(type), dataExpr(0) {
}

DysectAPI::DysectErrorCode Cond::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {

  if(conditionType == DataCondition) {
    if(dataExpr) {
      if(dataExpr->evaluate(result, process, tid) != DysectAPI::OK) {
        return DYSECTWARN(Error, "Could not evaluate data expression");
      }
    }
  } else if(conditionType == SyntheticCondition) {

    Synthetic* syn = dynamic_cast<Synthetic*>(this);

    if(syn->evaluate(result, process, tid) != DysectAPI::OK) {
        return DYSECTWARN(Error, "Could not evaluate synchetic expression");
    }

  } else if(conditionType == CVICondition) {

    CollectValuesIncludes* cvi = dynamic_cast<CollectValuesIncludes*>(this);

    if(cvi->evaluate(result, process, tid) != DysectAPI::OK) {
        return DYSECTWARN(Error, "Could not evaluate synchetic expression");
    }

  } else if(conditionType == AvgCondition) {

    AverageDeviates* cvi = dynamic_cast<AverageDeviates*>(this);

    if(cvi->evaluate(result, process, tid) != DysectAPI::OK) {
        return DYSECTWARN(Error, "Could not evaluate synchetic expression");
    }

  } else if(conditionType == BuckCondition) {

    BucketContains* bktcts = dynamic_cast<BucketContains*>(this);

    if(bktcts->evaluate(result, process, tid) != DysectAPI::OK) {
        return DYSECTWARN(Error, "Could not evaluate synchetic expression");
    }

  }

  return OK;
}

bool Cond::prepare() {
  DYSECTVERBOSE(true, "Prepare generic expression");
  return true;
}

Data::Data(string expr) : expr(expr), Cond(this) {
  etree = ExprTree::generate(expr);
}

string Data::getExpr() {
  return expr;
}

bool Data::prepare() {
  DYSECTVERBOSE(true, "Prepare data expression: %s", expr.c_str());
  return true;
}

DysectAPI::DysectErrorCode Data::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {
  assert(etree != 0);

  DYSECTVERBOSE(true, "Evaluate expression tree");
  if(etree->evaluate(result, process, tid) != OK) {
    return DYSECTVERBOSE(Error, "Evaluation failed");
  }

  DYSECTVERBOSE(true, "Evaluation done");

  return OK;
}

Synthetic::Synthetic(double ratio) : Cond(SyntheticCondition), ratio(ratio), procsIn(0) {
}

bool Synthetic::prepare() {
  return true;
}

DysectAPI::DysectErrorCode Synthetic::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {

  DYSECTVERBOSE(true, "Filter %d Procs in: %d", filterId, procsIn);

  if((procsIn % (int)ceil(1/ratio)) == 0) {
    DYSECTVERBOSE(true, "Let through");
    result = ResolvedTrue;
  } else {
    DYSECTVERBOSE(true, "Let go");
    result = ResolvedFalse;
  }

  procsIn++;

  return OK;
}

CollectValuesIncludes::CollectValuesIncludes(CollectValues* analysis, int value)
  : analysis(analysis), value(value), Cond(CVICondition) {

}

bool CollectValuesIncludes::prepare() {
  DYSECTVERBOSE(true, "Prepare CollectValuesIncludes with value: %d", value);
  return true;
}

DysectAPI::DysectErrorCode CollectValuesIncludes::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {
  assert(analysis != 0);

  DYSECTVERBOSE(true, "Searching global result for value %d", value);

  set<int>& globalVals = analysis->getGlobalResult()->getValues();
  if (globalVals.find(value) == globalVals.end()) {
    result = ResolvedFalse;
  } else {
    result = ResolvedTrue;
  }

  return OK;
}


AverageDeviates::AverageDeviates(Average* analysis, double deviation)
  : analysis(analysis), deviation(deviation), Cond(AvgCondition) {

}

bool AverageDeviates::prepare() {
  DYSECTVERBOSE(true, "Prepare AverageDeviates with value: %f", deviation);
  return true;
}

DysectAPI::DysectErrorCode AverageDeviates::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {
  assert(analysis != 0);

  //int rank = ProcMap::get()->getRank(process);
  std::map<int, Dyninst::ProcControlAPI::Process::ptr> *mpiRankToProcessMap;
  mpiRankToProcessMap = Domain::getMpiRankToProcessMap();
  if (!mpiRankToProcessMap) {
    DYSECTVERBOSE(false, "Could not find MPI rank map");
    return Error;
  }

  int rank = -1;
  std::map<int, Dyninst::ProcControlAPI::Process::ptr>::iterator iter;
  for (iter = mpiRankToProcessMap->begin(); iter != mpiRankToProcessMap->end(); iter++) {
    if (iter->second == process) {
      rank = iter->first;
      break;
    }
  }

  if (rank == -1) {
    DYSECTVERBOSE(false, "Failed to determine Rank");
    return Error;
  }

  double globalAvg = analysis->getGlobalResult()->getAverage();
  double localAvg = analysis->getAggregator()->getAverage(rank);
  double difference = fabs(globalAvg - localAvg);

  DYSECTVERBOSE(true, "Comparing difference (%f - %f =) %f with deviation %f", globalAvg, localAvg, difference, deviation);

  if (difference > deviation) {
    result = ResolvedTrue;
  } else {
    result = ResolvedFalse;
  }

  return OK;
}

BucketContains::BucketContains(Buckets* analysis, int bucket1, int bucket2)
  : analysis(analysis), bucket1(bucket1), bucket2(bucket2), Cond(BuckCondition) {

}

bool BucketContains::prepare() {
  DYSECTVERBOSE(true, "Prepare BucketContains with value: %d (%d)", bucket1, bucket2);
  return true;
}

DysectAPI::DysectErrorCode BucketContains::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {
  assert(analysis != 0);

  //int rank = ProcMap::get()->getRank(process);
  std::map<int, Dyninst::ProcControlAPI::Process::ptr> *mpiRankToProcessMap;
  mpiRankToProcessMap = Domain::getMpiRankToProcessMap();
  if (!mpiRankToProcessMap) {
    DYSECTVERBOSE(false, "Could not find MPI rank map");
    return Error;
  }

  int rank = -1;
  std::map<int, Dyninst::ProcControlAPI::Process::ptr>::iterator iter;
  for (iter = mpiRankToProcessMap->begin(); iter != mpiRankToProcessMap->end(); iter++) {
    if (iter->second == process) {
      rank = iter->first;
      break;
    }
  }

  if (rank == -1) {
    DYSECTVERBOSE(false, "Failed to determine Rank");
    return Error;
  }

  vector<RankBitmap*>& buckets = analysis->getGlobalResult()->getBuckets();
  if ((buckets[bucket1]->hasRank(rank)) ||
      (bucket2 != -1 && buckets[bucket2]->hasRank(rank))) {
    result = ResolvedTrue;
  } else {
    result = ResolvedFalse;
  }

  return OK;
}

