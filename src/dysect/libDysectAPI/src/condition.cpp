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

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;


int Synthetic::filterIdCounter = 0;

Cond* Data::eval(string expr) {
 Data* dobj = new Data(expr);

 Cond* cond = dynamic_cast<Cond*>(dobj);
 return cond;
}

Cond* Synthetic::prune(double ratio) {
 Err::verbose(true, "Create synthetic prune filter");
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
        return Err::warn(Error, "Could not evaluate data expression");
      }
    }
  } else if(conditionType == SyntheticCondition) {

    Synthetic* syn = dynamic_cast<Synthetic*>(this);

    if(syn->evaluate(result, process, tid) != DysectAPI::OK) {
        return Err::warn(Error, "Could not evaluate synchetic expression");
    }

  }

  return OK;
}

bool Cond::prepare() {
  Err::verbose(true, "Prepare generic expression");
  return true;
}

Data::Data(string expr) : expr(expr), Cond(this) {
  etree = ExprTree::generate(expr);
}

bool Data::prepare() {
  Err::verbose(true, "Prepare data expression: %s", expr.c_str());
  return true;
}

DysectAPI::DysectErrorCode Data::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {
  assert(etree != 0);

  Err::verbose(true, "Evaluate expression tree");
  if(etree->evaluate(result, process, tid) != OK) {
    return Err::verbose(Error, "Evaluation failed");
  }

  Err::verbose(true, "Evaluation done");

  return OK;
}

Synthetic::Synthetic(double ratio) : Cond(SyntheticCondition), ratio(ratio), procsIn(0) {
}

bool Synthetic::prepare() {
  return true;
}

DysectAPI::DysectErrorCode Synthetic::evaluate(ConditionResult& result, Process::const_ptr process, THR_ID tid) {

  Err::verbose(true, "Filter %d Procs in: %d", filterId, procsIn);

  if((procsIn % (int)ceil(1/ratio)) == 0) {
    Err::verbose(true, "Let through");
    result = ResolvedTrue;
  } else {
    Err::verbose(true, "Let go");
    result = ResolvedFalse;
  }

  procsIn++;
  

  return OK;
}
