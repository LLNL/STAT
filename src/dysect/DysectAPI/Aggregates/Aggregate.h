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

#ifndef __DYSECTAPI_AGGREGATE_h
#define __DYSECTAPI_AGGREGATE_h

#include <string>

enum agg_type {
  minAgg = 1,
  maxAgg = 2,
  // String aggregates
  funcLocAgg = 3,
  fileLocAgg = 4,
  paramNamesAgg = 5,
  tracesAgg = 6,
  staticStrAgg = 7,

  // Folds to aggregates
  descAgg = 8,

  rankListAgg = 9
};

namespace DysectAPI {
  class Agg {
  public:
    static bool aggregateIdFromName(std::string name, int& id);
  };
}

#endif
