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

#ifndef __CONDITION_H
#define __CONDITION_H

#include <string>

#include <DysectAPI/Aggregates/Aggregate.h>
#include <DysectAPI/Aggregates/Data.h>

namespace DysectAPI {
  class Cond;
  class Local;
  class Global;
  class Range;
  class Probe;
  class Data;
  class ExprTree;
  class Event;
  class TargetVar;
  class Function;
  class Value;
  
  typedef enum ConditionResult {
    Unresolved,
    Resolved,
    ResolvedTrue,
    ResolvedFalse,
    CollectiveResolvable,
    CollectiveResolvedTrue,
    CollectiveResolvedFalse
  } ConditionResult;

  class Cond {
    friend class Probe;

  protected:
    Probe* owner;

    Data* dataExpr;

    Cond(Data* dataExpr);

    enum ConditionType {
      UnknownCondition,
      DataCondition,
      SyntheticCondition
    } conditionType;

  public:
    Cond();

    Cond(ConditionType type);

    DysectErrorCode evaluate(ConditionResult& result, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);

    static Cond* And(Cond* first, Cond* second);
    static Cond* Or (Cond* first, Cond* second);
    static Cond* Not(Cond* cond);

    virtual bool prepare();
  };

  class DataRef {
    Value::content_t type;
    std::string name;

    TargetVar* var;
    
  public:
    DataRef() { }

    DataRef(Value::content_t type, const char *name);

    bool getVal(Value& val, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::ProcControlAPI::Thread::const_ptr thread);
  };

  class Synthetic : public Cond {

    static int filterIdCounter;

    int filterId;

    long procsIn;
    double ratio;

  public:
    Synthetic(double ratio);

    static Cond* prune(double ratio);
    DysectErrorCode evaluate(ConditionResult& result, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);

    bool prepare();
  };

  class Data : public Cond {
    std::string expr;
    ExprTree* etree;

  public:
    Data(std::string expr);

    static Cond* eval(std::string expr);
    DysectErrorCode evaluate(ConditionResult& result, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);

    bool prepare();
  };

  class CombinedCond : public Cond {
    typedef enum CondRel {
      AndRel,
      OrRel,
      NotRel,
    } CondRel;

    CondRel relation;
    Cond* first;
    Cond* second;

  public:
    CombinedCond(Cond* first, Cond* second, CondRel relation);
  };

	class Position {
	public:
		static Cond* in(Range* range);
		//static Cond* at(Location* location);
		static Cond* caller(Function* function);
		static Cond* onPath(Function* function);

    DysectErrorCode evaluate(ConditionResult &result);
    bool prepare();
	};
};

#endif
