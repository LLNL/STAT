#ifndef __EXPR_H
#define __EXPR_H

namespace DysectAPI {
  class ExprTree;
  class ExprNode;
  class TargetVar;
  class GlobalState;
  class InternalVar;
  class Value;

  class ExprTree {
    ExprNode* root;

  public:
    static ExprTree* generate(std::string expr);

    ExprTree(ExprNode* root);

    DysectErrorCode evaluate(ConditionResult& result, Dyninst::ProcControlAPI::Process::const_ptr process, THR_ID tid);
  };

  class TargetVar {
    std::string name;

    enum TargetVarType {
      Scalar,
      Derived,
      FieldList,
      Ranged
    };

    ExprNode* member;
    ExprNode* offset;
    ExprNode* derived;

    enum Value::content_t type;

  public:

    TargetVar(std::string name);

    std::string& getName();

    DysectErrorCode getValue(ConditionResult& result, Value& c, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);
  };

  class GlobalState {
  };

  class InternalVar {
  };

  class ExprNode {
    static int nodeIdCounter;

    int id;

    ExprNode* operands[2];

    Value* cval; //!< Constant value

    TargetVar* lnode; //!< Target variable

    GlobalState* gnode; //!< Global state
    
    InternalVar* inode; //!< Internal/Convenience variable

    DysectErrorCode containerHandler(ConditionResult& result, Value& c, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);
    DysectErrorCode exprTestHandler(ConditionResult& result, Value& c, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);
    DysectErrorCode exprLTestHandler(ConditionResult& result, Value& c, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);

    static bool isResolved(ConditionResult result);

  public:
    int getId() { return id; }

    enum ExprType {
      ExprContainer,
      ExprTest,
      ExprLTest,
      ExprArith,
      ExprBArith
    };

    enum ExprTestOp {
      TestEqual,
      TestNotEqual,
      TestLessThan,
      TestLessThanEqual,
      TestGreaterThan,
      TestGreaterThanEqual
    };

    enum ExprLTestOp {
      TestAnd,
      TestOr,
      TestNot
    };

    enum ExprArithOp {
      ExprAdd,
      ExprSub,
      ExprDiv,
      ExprMul
    };

    enum NodeType {
      undetermined,
      mixed,
      internal,
      target,
      global,
      value
    };

    ExprNode(ExprNode* op1, ExprNode* op2, ExprTestOp op);
    ExprNode(ExprNode* op1, ExprNode* op2, ExprLTestOp op);
    ExprNode(ExprNode* op1, ExprLTestOp op);
    ExprNode(long val);
    ExprNode(TargetVar* lnode);

    DysectErrorCode evaluate(ConditionResult& result, Value& c, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);
    DysectErrorCode prepare();

    DysectErrorCode getGlobalVars(std::vector<GlobalState*>& globals);

  private:
    enum ExprType opType;
    enum ExprTestOp testOp;
    enum ExprLTestOp ltestOp;
    enum ExprArithOp arithOp;
    enum NodeType nodeType;
  };
}

#endif
