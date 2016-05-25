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

#include <sys/stat.h>

#include "STAT.h"
#include <DysectAPI/DysectAPIFilter.h>
#include <DysectAPI/Aggregates/Aggregate.h>
#include <DysectAPI/Aggregates/AggregateFunction.h>

using namespace std;
using namespace DysectAPI;
using namespace MRN;

extern "C" {

struct packetAgg {
  struct packet* packet;
  int packetLen;
  int count;
};


  const char *dysectAPIUpStream_format_string = "%d %auc";

  void dysectAPIUpStream(vector<PacketPtr> &packetsIn,
      vector<PacketPtr> &packetsOut,
      vector<PacketPtr> &packetsOutReverse,
      void **filterState,
      PacketPtr &params,
      const TopologyLocalInfo &topology)
  {
    //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "dysectAPIUpStream entry\n");

    int streamId = packetsIn[0]->get_StreamId();
    map<int, struct packetAgg *> newPackets;
    map<int, struct packetAgg *>::iterator iter;
    vector<int> packetTagOrder;
    vector<int>::iterator vIter;
    vector<PacketPtr> myPacketsOut;
    map<int, PacketPtr> tagToPacket;
    map<int, PacketPtr>::iterator mIter;

    UpstreamFilter upstreamFilter(streamId);

    int newPacketLen = 0;

    struct timeval startTime, endTime;
    double elapsedTime = 0.0;

    gettimeofday(&startTime, NULL);

    for(int i = 0; i < packetsIn.size(); i++)
    {
      //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Incoming packet %d\n", i);
      PacketPtr currentPacket = packetsIn[i];

      int tag = currentPacket->get_Tag();
      int count;
      int payloadLen;
      char *payload;

      if(currentPacket->unpack("%d %auc", &count, &payload, &payloadLen) == -1) {
        cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Could not unpack packet!\n");
        continue;
      }

      // we process tags in receive order, but based on when the last packet from that tag is received
      vIter = find(packetTagOrder.begin(), packetTagOrder.end(), tag);
      if(vIter != packetTagOrder.end())
        packetTagOrder.erase(vIter);
      packetTagOrder.push_back(tag);

      //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Incoming packet tag %d, unpack count '%d' payload size %d\n", tag, count, payloadLen);

      if(payloadLen > 1) {
        //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handle probe packet with payload\n");

        if(newPackets.find(tag) == newPackets.end()) {
          //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting probe packet with base\n");
          newPackets[tag] = (struct packetAgg *)malloc(sizeof(packetAgg));
          newPackets[tag]->packet = (struct packet *)payload;
          newPackets[tag]->count = count;
          newPackets[tag]->packetLen = payloadLen;
        } else {
          newPackets[tag]->count += count;
          // Merge packets
          struct packet* mergedPacket = 0;
          AggregateFunction::mergePackets(newPackets[tag]->packet, (struct packet*)payload, mergedPacket, newPackets[tag]->packetLen);
          newPackets[tag]->packet = mergedPacket;
        }
      } else if(upstreamFilter.isControlTag(tag) && (payloadLen <= 1)) {
        cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handle control packet\n");
        upstreamFilter.aggregateControlPacket(tag, count);
      }

    }

    for(iter = newPackets.begin(); iter != newPackets.end(); iter++) {
      packetAgg *newPacket = iter->second;
      //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting probe packet tag %d with base (npl: %d)\n", iter->first, newPacket->packetLen);
      PacketPtr packet(new Packet(streamId, iter->first, "%d %auc", newPacket->count, (unsigned char*)newPacket->packet, newPacket->packetLen));
      tagToPacket[iter->first] = packet;
    }

    if(upstreamFilter.anyControlPackets()) {
      upstreamFilter.getControlPackets(tagToPacket);
    }

    for(vIter = packetTagOrder.begin(); vIter != packetTagOrder.end(); vIter++)
      packetsOut.push_back(tagToPacket[*vIter]);

    gettimeofday(&endTime, NULL);

    elapsedTime += (endTime.tv_sec -  startTime.tv_sec) * 1000.0;
    elapsedTime += (endTime.tv_usec - startTime.tv_usec) / 1000.0;

    cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Aggregated %d packets in %.3f ms\n", packetsIn.size(), elapsedTime);
  }

  const char *dysectAPIDownStream_format_string = "%d %auc";

  void dysectAPIDownStream(vector<PacketPtr> &packetsIn,
      vector<PacketPtr> &packetsOut,
      vector<PacketPtr> &packetsOutReverse,
      void **filterState,
      PacketPtr &params,
      const TopologyLocalInfo &topology)
  {

  }
}


UpstreamFilter::UpstreamFilter(int streamId) :  numControlTags(10),
  inactive(-1),
  controlTagTemplate(0),
  controlPacketsAdded(false),
  streamId(streamId) {
    controlStatus = new int[numControlTags + 1];

    for(int i = 0; i <= numControlTags; i++) {
      controlStatus[i] = inactive;
    }
  }

UpstreamFilter::~UpstreamFilter() {
}


bool UpstreamFilter::isControlTag(int tag) {
  int controlTag = indexFromControlTag(tag);

  return (controlTag < numControlTags);
}

int UpstreamFilter::indexFromControlTag(int tag) {
  return tag & 0xff;
}

bool UpstreamFilter::aggregateControlPacket(int tag, int count) {
  if(!isControlTag(tag)) {
    return false;
  }

//  vector<int>::iterator iter;
  int controlTag = indexFromControlTag(tag);
  controlPacketsAdded = true;

  if(controlTagTemplate == 0) {
    controlTagTemplate = (tag & (-1 ^ 0xff));
  }

  if(controlStatus[controlTag] == inactive) {
    controlStatus[controlTag] = 0;
  }

  controlStatus[controlTag] += count;

//  iter = find(controlPacketOrder.begin(), controlPacketOrder.end(), controlTag);
//  if(iter != controlPacketOrder.end())
//    controlPacketOrder.erase(iter);
//  controlPacketOrder.push_back(controlTag);

  //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Aggregated control packet tag %d, count %d, controlTag %d controlTagTemplate %d\n", tag, count, controlTag, controlTagTemplate);

  controlPacketsAdded = true;

  return true;
}

bool UpstreamFilter::getControlPackets(std::map<int, MRN::PacketPtr>& packets) {
  int i;
  for (i = 0; i < numControlTags; i++) {
    int count = controlStatus[i];

    if(count != inactive) {
      int newControlTag = controlTagTemplate | i;
      cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Producing control packet %d\n", newControlTag);
      PacketPtr packet(new Packet(streamId, newControlTag, "%d %auc", count, "", 1));
      packets[newControlTag] = packet;
    }
  }

  return true;
}

bool UpstreamFilter::anyControlPackets() {
  return controlPacketsAdded;
}
