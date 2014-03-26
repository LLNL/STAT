#ifndef __DYSECTAPI_CMP_AGG_H
#define __DYSECTAPI_CMP_AGG_H

namespace DysectAPI {
  class CmpAgg : public AggregateFunction {
  protected:
    va_list args_;
    std::string fmt_;
    std::vector<DataRef*> params_;
  
    Value curVal;

    virtual bool compare(Value& newVal, Value& oldVal) = 0;

  public:
    CmpAgg(agg_type ltype, int id, int count, std::string fmt, void* payload);
    CmpAgg(agg_type ltype, std::string fmt);
    
    Value& getVal() { return curVal; }
    
    bool aggregate(AggregateFunction* agg);
    
    bool collect(void* process, void* thread);
    
    bool print();
    bool getStr(std::string& str);

    bool clear();
    
    int getSize();
    
    int writeSubpacket(char *p);

    bool isSynthetic();
    bool getRealAggregates(std::vector<AggregateFunction*>& aggregates);
    bool fetchAggregates(Probe* probe);
  };

  class Min : public CmpAgg {
  protected:
    bool compare(Value& lhs, Value& rhs);

  public:
    Min(int id, int count, std::string fmt, void* payload);
    Min(std::string fmt, ...);
  };

  class Max : public CmpAgg {
  protected:
    bool compare(Value& lhs, Value& rhs);

  public:
    Max(int id, int count, std::string fmt, void* payload);
    Max(std::string fmt, ...);
  };
}

#endif
