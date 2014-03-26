#ifndef __CONDITION_H
#define __CONDITION_H

namespace DysectAPI {
  class Cond;
  class Local;
  class Global;
  class Range;
  class Probe;
  class Cond;
  class Data;
  class ExprTree;
  class Event;
  class TargetVar;
  
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
