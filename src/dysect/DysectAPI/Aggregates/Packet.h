#ifndef __DYSECTAPI_PACKET_H
#define __DYSECTAPI_PACKET_H

namespace DysectAPI {
  const int maxFmt = 16;
  
  struct subPacket {
    int len;
    int id;
    int count;
    int type;
    char fmt[maxFmt];
    char* payload;
  };
  
  struct packet {
    int num;
    char* payload;
  };
}

#endif
