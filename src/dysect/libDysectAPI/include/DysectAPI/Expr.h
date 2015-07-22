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

#ifndef __EXPR_H
#define __EXPR_H

#include <string>
#include <vector>

#include <DysectAPI/Condition.h>

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

    DysectErrorCode evaluate(ConditionResult& result, Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::THR_ID tid);
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
