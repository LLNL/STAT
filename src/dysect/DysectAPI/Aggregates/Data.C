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

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI/Aggregates/Data.h"

using namespace std;
using namespace DysectAPI;

Value::Value(std::string fmt, void *lbuf) {
  int slen = fmt.size();
  const char *str = fmt.c_str();

  if((slen > 1) || (slen < 3)) {
    if(str[0] == '%') {
      if(str[1] == 'd') {
        len = sizeof(int);
        content = intType;

      } else if(str[1] == 'l') {
        len = sizeof(long);
        content = longType;

      } else if(str[1] == 'f') {
        len = sizeof(float);
        content = floatType;

      } else if(str[1] == 'L') {
        len = sizeof(double);
        content = doubleType;

      } else if(str[1] == 'p') {
        len = sizeof(long);
        content = pointerType;

      } else {
        fprintf(stderr, "Unknown format: %s\n", fmt.c_str());
      }
    }
  }

  buf = lbuf;
}

Value::Value(const Value& copy) : content(copy.content), len(copy.len) {
  // Create a new buffer and copy the content
  buf = malloc(copy.len);
  memcpy(buf, copy.buf, copy.len);
}

Value::Value() : content(noType), len(0), buf(0) {}

Value::Value(float fval) : content(floatType), len(0), buf(0){
  populate<float>(fval);
}

Value::Value(double dval) : content(doubleType), len(0), buf(0){
  populate<double>(dval);
}

Value::Value(int ival) : content(intType), len(0), buf(0){
  populate<int>(ival);
}

Value::Value(long lval) : content(longType), len(0), buf(0){
  populate<long>(lval);
}

Value::Value(bool bval) : content(boolType), len(0), buf(0) {
  populate<bool>(bval);
}

char *Value::getFmt()
{
  if (content == intType)
    return "%d";
  else if (content == longType)
    return "%l";
  else if (content == floatType)
    return "%f";
  else if (content == doubleType)
    return "%L";
  else if (content == pointerType)
    return "%p";
  else
    return "?";
}

void Value::copy(const Value& rhs) {
  if(this == &rhs) {
    return ;
  }

  if(rhs.len > len) {
    free(buf);
    buf = malloc(rhs.len);
    len = rhs.len;
  } else {
    memset(buf, '\0', len);
  }

  memcpy(buf, rhs.buf, rhs.len);

  len = rhs.len;
  content = rhs.content;
};

Value::content_t Value::getType() {
  return content;
}

int Value::compare(Value& c) {
  int ret = 0;
  content_t domType = Value::noType;

  if(c.content > content) {
    domType = c.content;
  } else {
    domType = content;
  }

  switch(content) {
    case boolType: {
                     bool op1 = getValue<bool>();
                     bool op2 = c.getValue<bool>();

                     if(op1 == op2)
                       return eq;
                   }
                   break;
    case intType: {

                    int op1 = getValue<int>();
                    int op2 = c.getValue<int>();

                    int diff = op1 - op2;

                    if(diff == 0)
                      return eq;

                    if(diff < 0)
                      return lt;

                    if(diff > 0)
                      return gt;
                  }
                  break;
    case longType: {
                     long op1 = getValue<long>();
                     long op2 = c.getValue<long>();

                     long diff = op1 - op2;

                     if(diff == 0)
                       return eq;

                     if(diff < 0)
                       return lt;

                     if(diff > 0)
                       return gt;
                   }
                   break;
    case floatType: {
                     float op1 = getValue<float>();
                     float op2 = c.getValue<float>();

                     float diff = op1 - op2;

                     if(diff == 0.0)
                       return eq;

                     if(diff < 0.0)
                       return lt;

                     if(diff > 0.0)
                       return gt;
                   }
                   break;
    case doubleType: {
                     double op1 = getValue<double>();
                     double op2 = c.getValue<double>();

                     double diff = op1 - op2;

                     if(diff == 0.0)
                       return eq;

                     if(diff < 0.0)
                       return lt;

                     if(diff > 0.0)
                       return gt;
                   }
                   break;
    case pointerType: {
                     long op1 = getValue<long>();
                     long op2 = c.getValue<long>();

                     long diff = op1 - op2;

                     if(diff == 0)
                       return eq;

                     if(diff < 0)
                       return lt;

                     if(diff > 0)
                       return gt;
                   }
                   break;
    default:
                   cerr << "Type not yet supported and values cannot be compared" << endl;
                   break;
  }

  return 0;
}

bool Value::isEqual(Value& c) {
  int ret = compare(c);

  return((ret & eq) == eq);
}

bool Value::isNotEqual(Value& c) {
  int ret = compare(c);

  return((ret & eq) != eq);
}

bool Value::isLessThan(Value& c) {
  int ret = compare(c);

  return((ret & lt) == lt);
}

bool Value::isLessThanEqual(Value& c) {
  int ret = compare(c);

  return (((ret & lt) == lt) || ((ret & eq) == eq));
}

bool Value::isGreaterThan(Value& c) {
  int ret = compare(c);

  return((ret & gt) == gt);
}

bool Value::isGreaterThanEqual(Value& c) {
  int ret = compare(c);

  return (((ret & gt) == gt) || ((ret & eq) == eq));
}

bool Value::getStr(string& str) {
  char outBuf[32];

  // XXX: Replace with memory copy
  switch(content) {
    case intType:
      snprintf((char*)&outBuf, 32, "%d",  *(int*)buf);
      break;
    case longType:
      snprintf((char*)&outBuf, 32, "%ld",  *(long*)buf);
      break;
    case floatType:
      snprintf((char*)&outBuf, 32, "%f",  *(float*)buf);
      break;
    case doubleType:
      snprintf((char*)&outBuf, 32, "%f",  *(double*)buf);
      break;
    case pointerType:
      snprintf((char*)&outBuf, 32, "%lx", *(long*)buf);
      break;

    default:
      snprintf((char*)&outBuf, 32, "unknown type %d", content);
      break;
  }

  str = std::string((char*)&outBuf);

  return true;
}

bool Value::isLongLike() {
  switch (content) {
    case intType:
    case longType:
    case pointerType:
      return true;

    default:
      return false;
  }
}

bool Value::isDoubleLike() {
  switch (content) {
    case floatType:
    case doubleType:
      return true;

    default:
      return false;
  }
}

long Value::asLong() {
  switch (content) {
    case intType:
    case longType:
    case pointerType:
      return getValue<long>();

    default:
      return 0;
  }
}

double Value::asDouble() {
  switch (content) {
    case floatType:
    case doubleType:
      return getValue<double>();

    default:
      return 0.0;
  }
}

Value Value::operator+(Value& rhs) {
  Value res;

  switch (content) {
    case intType:
      res.populate<int>(getValue<int>() + rhs.getValue<int>());
      res.setType(intType);
      return res;
      
    case longType:
      res.populate<long>(getValue<long>() + rhs.getValue<long>());
      res.setType(longType);
      return res;
      
    case pointerType:
      res.populate<void*>((void*)(getValue<long>() + rhs.getValue<long>()));
      res.setType(pointerType);
      return res;
      
    case floatType:
      res.populate<float>(getValue<float>() + rhs.getValue<float>());
      res.setType(floatType);
      return res;
      
    case doubleType:
      res.populate<double>(getValue<double>() + rhs.getValue<double>());
      res.setType(doubleType);
      return res;
      
    default:
      cerr << "Cannot add booleans!" << endl;
  }

  return *this;
}

bool Value::operator<=(Value& rhs) {
  return isLessThanEqual(rhs);
}

bool Value::operator>=(Value& rhs) {
  return isGreaterThanEqual(rhs);
}

Value& Value::operator=(Value& rhs) {
  copy(rhs);
  return rhs;
}

bool Value::operator<(Value& rhs) {
  return isLessThan(rhs);
}

bool Value::operator>(Value& rhs) {
  return isGreaterThan(rhs);
}
