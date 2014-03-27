#ifndef __DYSECTAPI_DESC_VAR_H
#define __DYSECTAPI_DESC_VAR_H

namespace DysectAPI {
  class Probe;

  class DescribeVariable : public AggregateFunction {
    std::string varName;
    std::vector<std::string> varSpecs;
    std::map<int, AggregateFunction*> aggregates;
    StaticStr* strAgg;

    std::string outStr;
    std::vector<std::string> output;

  public:
    DescribeVariable(std::string name);

    int getSize();
    int writeSubpacket(char *p);
    bool collect(void* process, void* thread);
    bool clear();

    bool getStr(std::string& str);
    bool print();
    bool aggregate(AggregateFunction* agg);

    bool isSynthetic();
    bool getRealAggregates(std::vector<AggregateFunction*>& aggregates);
    bool fetchAggregates(Probe* probe);
  };
}

#endif
