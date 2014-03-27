#ifndef __DYSECTAPI_AGGREGATE_FUNCTION_H
#define __DYSECTAPI_AGGREGATE_FUNCTION_H

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
    
    AggregateFunction(agg_type type) : type(type), count_(0) { id_ = genId(); }
    AggregateFunction() : count_(0) {}
    
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
