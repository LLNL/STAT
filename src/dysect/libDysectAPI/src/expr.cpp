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
#include <expr-parser.h>
#include <expr-scanner.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace Stackwalker;

//
// Parser/Lexer externs
//

extern int yyparse();
extern ExprNode* parsedRoot;
ExprNode* parsedRoot;

//
// ExprNode class static
//
int ExprNode::nodeIdCounter = 0;


ExprTree* ExprTree::generate(string expr) {
  const int bufSize = 512;
  char buf[bufSize];
  
  snprintf((char*)&buf, bufSize, "%s", expr.c_str());

  DYSECTVERBOSE(true, "Parsing '%s'", (char*)&buf);

  // XXX: Parse into lex local context instead of global one
  yy_scan_string((char*)&buf);

  parsedRoot = 0;

  yyparse();

  if(parsedRoot == 0) {
    DYSECTWARN(Error, "Could not generate expression tree!");
    return 0;
  }

  ExprTree* treePtr = new ExprTree(parsedRoot);
  parsedRoot = 0;

  return treePtr;
}

ExprNode::ExprNode(ExprNode* op1, ExprNode* op2, ExprTestOp op) : opType(ExprTest), testOp(op), nodeType(undetermined) {
  id = nodeIdCounter++;

  operands[0] = op1;
  operands[1] = op2;
}

ExprNode::ExprNode(ExprNode* op1, ExprNode* op2, ExprLTestOp lop) : opType(ExprLTest), 
                                                                    ltestOp(lop), 
                                                                    nodeType(undetermined) {
  id = nodeIdCounter++;

  operands[0] = op1;
  operands[1] = op2;
}

ExprNode::ExprNode(ExprNode* op1, ExprLTestOp lop) : opType(ExprLTest), 
                                                     ltestOp(lop), 
                                                     nodeType(undetermined) {
  id = nodeIdCounter++;

  operands[0] = op1;
  operands[1] = 0;
}

ExprNode::ExprNode(long lval) : opType(ExprContainer), nodeType(value) {
  id = nodeIdCounter++;

  cval = new Value(lval);

  operands[0] = 0;
  operands[1] = 0;
}

ExprNode::ExprNode(TargetVar* lnode) : opType(ExprContainer), nodeType(target), lnode(lnode) {
  id = nodeIdCounter++;

  operands[0] = 0;
  operands[1] = 0;
}

TargetVar::TargetVar(std::string name) : name(name) {
}

string& TargetVar::getName() {
  return name;
}

ExprTree::ExprTree(ExprNode* root) : root(root) {
}


DysectAPI::DysectErrorCode ExprNode::evaluate(ConditionResult& result, Value& c, Process::const_ptr process, THR_ID tid) {
  DysectAPI::DysectErrorCode code;

  switch(opType) {
    case ExprContainer:
      DYSECTVERBOSE(true, "Value node");
      code = containerHandler(result, c, process, tid);
      break;

    case ExprTest: 
      DYSECTVERBOSE(true, "Evaluating test");
      code = exprTestHandler(result, c, process, tid);
      break;

    case ExprLTest:
      DYSECTVERBOSE(true, "Logic test");
      code = exprLTestHandler(result, c, process, tid);
      break;

    case ExprBArith:
      return DYSECTWARN(Error, "Boolean arithmetic expressions not yet supported");
      break;
    case ExprArith:
      return DYSECTWARN(Error, "Arithmetic expressions not yet supported");
      break;
    default:
      return DYSECTWARN(Error, "Unknown operation for expression");
      break;
  }

  return code;
}

DysectAPI::DysectErrorCode ExprNode::containerHandler(ConditionResult& result, Value& c, Process::const_ptr process, THR_ID tid) {
  DysectAPI::DysectErrorCode code;

  switch(nodeType) {
    case target: 
      if(!lnode) {
        return DYSECTWARN(Error, "Node type is local node, but node data not present");
      }

      code = lnode->getValue(result, c, process, tid);
      if(code != DysectAPI::OK) {
        return DYSECTWARN(code, "Could not get value for local node");
      }
      break;

    case value: 
      DYSECTVERBOSE(true, "Constant value found");
      c.copy(*cval);

      if(cval->getType() == Value::boolType) {
        if(cval->getValue<bool>()) {
          result = ResolvedTrue;
        } else {
          result = ResolvedFalse;
        }
      } else {
        result = Resolved;
      }

      break;

    case internal:
    case global:
      return DYSECTWARN(Error, "Node type not yet supported");
      break;
  }

  return OK;
}

DysectAPI::DysectErrorCode ExprNode::exprTestHandler(ConditionResult& result, Value& c, Process::const_ptr process, THR_ID tid) {
  if((operands[0] == 0) || (operands[1] == 0)) {
    return DYSECTWARN(Error, "2 operands are required for test");
  }

  Value c1, c2;

  ConditionResult r1, r2;

  if( (operands[0]->evaluate(r1, c1, process, tid) != DysectAPI::OK)  || 
      (operands[1]->evaluate(r2, c2, process, tid) != DysectAPI::OK)) {
    return DYSECTWARN(Error, "Operand could not be evaluated");
  }

  if((!isResolved(r1)) || (!isResolved(r2))) {
    return DYSECTWARN(Error, "Can't handle unresolved subtrees yet"); 
  }

  switch(testOp) {
    case TestEqual:
      result = (c1.isEqual(c2) ? ResolvedTrue : ResolvedFalse);
      break;
    case TestNotEqual:
      result = (c1.isNotEqual(c2) ? ResolvedTrue : ResolvedFalse);
      break;
    case TestLessThan:
      result = (c1.isLessThan(c2) ? ResolvedTrue : ResolvedFalse);
      break;
    case TestLessThanEqual:
      result = (c1.isLessThanEqual(c2) ? ResolvedTrue : ResolvedFalse);
      break;
    case TestGreaterThan:
      result = (c1.isGreaterThan(c2) ? ResolvedTrue : ResolvedFalse);
      break;
    case TestGreaterThanEqual:
      result = (c1.isGreaterThanEqual(c2) ? ResolvedTrue : ResolvedFalse);
      break;
    default:
      return DYSECTWARN(Error, "Test not yet supported");
      break;
  }


  bool val = (result == ResolvedTrue);

  DYSECTVERBOSE(true, "Test evaluated: %s", val ? "true" : "false");

  Value retVal(val);

  DYSECTVERBOSE(true, "New value is of type %d", retVal.getType());
  
  c.copy(retVal);

  DYSECTVERBOSE(true, "Copied value type %d", c.getType());

  return OK;
}

DysectAPI::DysectErrorCode ExprNode::exprLTestHandler(ConditionResult& result, Value& c, Process::const_ptr process, THR_ID tid) {
  if(operands[0] == 0) {
    return DYSECTWARN(Error, "At least 1 operand is required for logical test");
  }

  Value c1, c2;

  ConditionResult r1, r2;

  if(operands[0]->evaluate(r1, c1, process, tid) != DysectAPI::OK) {
    return DYSECTWARN(Error, "Operand could not be evaluated");
  }

  if(!isResolved(r1)) {
    return DYSECTWARN(Error, "Can't handle unresolved subtrees yet");
  }
  
  if(c1.getType() != Value::boolType) {
    DYSECTVERBOSE(Error, "Comparing non-boolean value (operand 1): %d", c1.getType());
  }

  if(ltestOp != TestNot) {
    if(operands[1]->evaluate(r2, c2, process, tid) != DysectAPI::OK) {
      return DYSECTVERBOSE(Error, "Operand could not be evaluated");
    }

    if(!isResolved(r2)) {
      return DYSECTVERBOSE(Error, "Can't handle unresolved subtrees yet");
    }
    
    if(c2.getType() != Value::boolType) {
      DYSECTVERBOSE(Error, "Comparing non-boolean value (operand 2): %d", c2.getType());
    }
  }
  
  bool op1val = false;
  bool op2val = false;

  switch(ltestOp) {
    case TestAnd:
      op1val = c1.getValue<bool>();
      op2val = c2.getValue<bool>();

      result = (op1val && op2val) ? ResolvedTrue : ResolvedFalse;
      break;
    case TestOr:
      op1val = c1.getValue<bool>();
      op2val = c2.getValue<bool>();

      result = (op1val || op2val) ? ResolvedTrue : ResolvedFalse;
      break;
    case TestNot:
      op1val = c1.getValue<bool>() ? ResolvedTrue : ResolvedFalse;

      result = (op1val == false) ? ResolvedTrue : ResolvedFalse;
      break;
    default:
      return DYSECTWARN(Error, "Logical test not yet supported");
      break;
  }

  bool val = (result == ResolvedTrue);
  Value retVal(val);
  c.copy(retVal);

  return OK;
}


bool ExprNode::isResolved(ConditionResult result) {
  return ((result == Resolved) || (result == ResolvedTrue) || (result == ResolvedFalse));
}
