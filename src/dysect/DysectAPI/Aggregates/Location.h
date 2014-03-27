#ifndef __DYSECTAPI_LOCATION_H
#define __DYSECTAPI_LOCATION_H

namespace DysectAPI {
  class FuncLocation : public StrAgg {
     
  public:
    FuncLocation(int id, int count, std::string fmt, void* payload);
    FuncLocation();

    bool collect(void* process, void* thread);
    
    bool print();
  };

  class FileLocation : public StrAgg {
  public:
    FileLocation(int id, int count, std::string fmt, void* payload);
    FileLocation();

    bool collect(void* process, void* thread);
    
    bool print();
  };

  class FuncParamNames : public StrAgg {
    std::vector<AggregateFunction*> paramBounds;

  public:
    
    FuncParamNames(int id, int count, std::string fmt, void* payload);
    FuncParamNames();

    bool collect(void* process, void* thread);
    
    bool print();
  };

  class StackTraces : public StrAgg {
  public:
    
    StackTraces(int id, int count, std::string fmt, void* payload);
    StackTraces();

    bool collect(void* process, void* thread);
    
    bool print();
  };

  class StaticStr : public StrAgg {
    
  public:
    StaticStr(int id, int count, std::string fmt, void* payload);
    StaticStr();

    bool addStr(std::string str);

    bool collect(void* process, void* thread);

    bool print();
  };
}

#endif
