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

#ifndef __DYSECTAPI_FILTER_H
#define __DYSECTAPI_FILTER_H

extern "C" void cpPrintMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...);

namespace DysectAPI {
  class UpstreamFilter;
  class AggPacket;

  class UpstreamFilter {
    const int streamId;

    const int numControlTags;
    const int inactive;

    bool controlPacketsAdded;
    int* controlStatus;
    int controlTagTemplate;

  public:
    UpstreamFilter(int streamId);
    ~UpstreamFilter();

    bool aggregateProbePacket(int tag, int count, char* payload);

    bool isControlTag(int tag);
    int  indexFromControlTag(int tag);
    bool aggregateControlPacket(int tag, int count);
    bool getControlPackets(std::vector<MRN::PacketPtr>& packets);
    bool anyControlPackets();
  };
}

#endif
