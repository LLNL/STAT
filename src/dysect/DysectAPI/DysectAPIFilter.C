#include "STAT.h"
#include "graphlib.h"
#include <sys/stat.h>

#include <DysectAPI/DysectAPIFilter.h>
#include <DysectAPI/Aggregates/Aggregate.h>

using namespace std;
using namespace DysectAPI;
using namespace MRN;

extern "C" {

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
    int tag = packetsIn[0]->get_Tag();

    UpstreamFilter upstreamFilter(streamId);

    struct packet* newPacket = 0;
    int newPacketLen = 0;
    int newCount = 0;

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

      //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Incoming packet unpack count '%d' payload size %d\n", count, payloadLen);

      if(payloadLen > 1) {
        //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handle probe packet with payload\n");

        if(newPacket == 0) {
          newCount = count;
          
          //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting probe packet with base\n");
          newPacket = (struct packet*)payload;
          newPacketLen = payloadLen;
        } else {
          newCount += count;
          // Merge packets
          struct packet* mergedPacket = 0;
          AggregateFunction::mergePackets(newPacket, (struct packet*)payload, mergedPacket, newPacketLen);

          newPacket = mergedPacket;
        }
      } else if(upstreamFilter.isControlTag(tag) && (payloadLen <= 1)) {
        cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handle control packet\n");
        upstreamFilter.aggregateControlPacket(tag, count);
      }

    }

    if(newPacket != 0) {
      //cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting probe packet with base (npl: %d)\n", newPacketLen);
      PacketPtr packet(new Packet(streamId, tag, "%d %auc", newCount, (unsigned char*)newPacket, newPacketLen));
      packetsOut.push_back(packet);
    }

    if(upstreamFilter.anyControlPackets()) {
      upstreamFilter.getControlPackets(packetsOut);
    }

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

  int controlTag = indexFromControlTag(tag);
  controlPacketsAdded = true;

  if(controlTagTemplate == 0) {
    controlTagTemplate = (tag & (-1 ^ 0xff));
  }

  if(controlStatus[controlTag] == inactive) {
    controlStatus[controlTag] = 0;
  }

  controlStatus[controlTag] += count;

  controlPacketsAdded = true;

  return true;
}

bool UpstreamFilter::getControlPackets(std::vector<MRN::PacketPtr>& packets) {
  for(int i = 0; i < numControlTags; i++) {
    int count = controlStatus[i];

    if(count != inactive) {
      int newControlTag = controlTagTemplate | i;


      cpPrintMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Producing control packet\n");
      PacketPtr packet(new Packet(streamId, newControlTag, "%d %auc", count, "", 1));
      packets.push_back(packet);
    }
  }

  return true;
}

bool UpstreamFilter::anyControlPackets() {
  return controlPacketsAdded;
}
