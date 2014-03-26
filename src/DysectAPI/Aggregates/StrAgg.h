#ifndef __DYSECTAPI_STRAGG_H
#define __DYSECTAPI_STRAGG_H

namespace DysectAPI {
  class StrAgg : public AggregateFunction {
  protected:
    int strsLen;

    std::map<std::string, int> countMap;

    void* payloadEnd;

    bool deserialize(void *payload);
    bool serialize(std::map<std::string, int>& strCountMap);
     
  public:
    StrAgg(agg_type ltype, int id, int count, std::string fmt, void* payload);
    StrAgg(agg_type ltype);

    std::map<std::string, int>& getCountMap();
    
    bool aggregate(AggregateFunction* agg);
    
    bool clear();
    
    int getSize();
    
    int writeSubpacket(char *p);

    virtual bool collect(void* process, void* thread) = 0;
    
    virtual bool print() = 0;

    bool getStr(std::string& str);

    void getCountMap(std::map<std::string, int>& strCountMap);

    bool isSynthetic();
    bool getRealAggregates(std::vector<AggregateFunction*>& aggregates);
    bool fetchAggregates(Probe* probe);
  };
}

#endif
