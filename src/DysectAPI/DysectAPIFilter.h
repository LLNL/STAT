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
