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

#ifndef __DYSECTAPI_AVERAGEAGG_H
#define __DYSECTAPI_AVERAGEAGG_H

#include <string>
#include <vector>
#include <map>

#include "DysectAPI/Aggregates/AggregateFunction.h"

namespace DysectAPI {
  class AverageAgg : public AggregateFunction {
  protected:
    double average;
    double count;

    std::map<int, double> procAvg;

  public:
    AverageAgg(int id, int count, std::string fmt, void* payload);
    AverageAgg(Probe *inOwner);
    AverageAgg();
    void copy(AverageAgg* other);

    void addValue(double average, double count, int rank);
    void addValue(double average, double count);
    double getAverage(int rank);
    double getAverage();
    double getCount();
    
    int getSize();
    bool deserialize(void *payload);
    int writeSubpacket(char *p);
    bool readSubpacket(char* p);
    virtual bool collect(void* process, void* thread);
    bool clear();
    
    bool getStr(std::string& str);
    virtual bool print();
    bool aggregate(AggregateFunction* agg);

    bool isSynthetic();
    bool getRealAggregates(std::vector<AggregateFunction*>& aggregates);
    bool fetchAggregates(Probe* probe);
  };
}

#endif
