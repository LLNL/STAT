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

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool CmpAgg::collect(void *process, void *thread) {
  Process::const_ptr* process_ptr = (Process::const_ptr*)process;
  Thread::const_ptr* thread_ptr = (Thread::const_ptr*)thread;

  if(params_.size() != 1) {
    fprintf(stderr, "Compare aggregate takes one numeric value\n");
    return false;
  }
  
  DataRef* ref = params_[0];
  Value val;
  
  if(!ref->getVal(val, *process_ptr, *thread_ptr)) {
    fprintf(stderr, "Could not get value for data reference\n");
    return false;
  }

  string str;
  val.getStr(str);
  Err::verbose(true, "Read value is %s", str.c_str());
  
  if(curVal.getType() == Value::noType) {
    curVal = val;
  } else {
    if(compare(val, curVal))
      curVal = val;
  }

  count_++;
  
  return true;
}

Min::Min(std::string fmt, ...) : CmpAgg(minAgg, fmt) {
  va_list args;
  va_start(args, fmt);
  va_copy(args_, args);
  va_end(args);
  
  getParams(fmt, params_, args_);
}

Max::Max(std::string fmt, ...) : CmpAgg(maxAgg, fmt) {
  va_list args;
  va_start(args, fmt);
  va_copy(args_, args);
  va_end(args);
  
  getParams(fmt, params_, args_);
}
