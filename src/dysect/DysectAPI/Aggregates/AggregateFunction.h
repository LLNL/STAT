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

#ifndef __DYSECTAPI_AGGREGATE_FUNCTION_H
#define __DYSECTAPI_AGGREGATE_FUNCTION_H

#include <string>
#include <map>
#include <vector>
#include <stdarg.h>

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI/Aggregates/Packet.h"

namespace DysectAPI {
  class Probe;
  class DataRef;

  class AggregateFunction {
    friend class DescribeVariable;

    static long counterId;
    Probe* owner;
  
  protected:
    long id_;
    agg_type type;
    int count_;

    long genId() { return counterId++; }
    bool getParams(std::string fmt, std::vector<DataRef*>& params, va_list args);
    
  public:
    static bool getPacket(std::vector<AggregateFunction*>& aggregates, int& len, struct packet*& ptr);  
    static bool mergePackets(struct packet* ptr1, struct packet* ptr2, struct packet*& ptr3, int& len);
  
    static int getAggregate(char *p, AggregateFunction*& aggFunc);
    static bool getAggregates(std::map<int, AggregateFunction*>& aggregates, struct packet* ptr) ;  
    static void resetCounterId();
    
    AggregateFunction(agg_type type) : type(type), count_(0) { id_ = genId(); }
    AggregateFunction(Probe *inOwner) : owner(inOwner), count_(0) { id_ = genId(); }
    AggregateFunction() : count_(0) {}

    Probe *getOwner() { return owner; }
    
    long getId() { return id_; }
    agg_type getType() { return type; }
    int getCount() { return count_; }
  
    virtual int getSize() = 0;
    virtual int writeSubpacket(char *p) = 0;
    virtual bool collect(void* process, void* thread) = 0;
    virtual bool clear() = 0;

    virtual bool getStr(std::string& str) = 0;
    virtual bool print() = 0;
    virtual bool aggregate(AggregateFunction* agg) = 0;

    virtual bool isSynthetic() = 0;
    virtual bool getRealAggregates(std::vector<AggregateFunction*>& aggregates) = 0;
    virtual bool fetchAggregates(Probe* probe) = 0;
  };
}

#endif
