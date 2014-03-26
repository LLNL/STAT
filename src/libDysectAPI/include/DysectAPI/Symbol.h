#ifndef __SYMBOL_H
#define __SYMBOL_H

namespace DysectAPI {
  class CodeLocation;
  class DataLocation;

  class SymbolTable {
    static std::map<std::string, Dyninst::SymtabAPI::Symtab*> symtabMap;

  public:
    static DysectErrorCode getSymbolTable(std::string name, Dyninst::SymtabAPI::Symtab*& symtab);
    static DysectErrorCode getLibraries(std::vector<Dyninst::Stackwalker::LibAddrPair>& libs, Dyninst::Stackwalker::Walker* proc);
    static DysectErrorCode getLibraryByAddr(Dyninst::Stackwalker::LibAddrPair& lib, Dyninst::Address addr, Dyninst::Stackwalker::Walker* proc);
    static DysectErrorCode getFileLineByAddr(std::string& fileName, int& line, Dyninst::Address addr, Dyninst::Stackwalker::Walker* proc);

    static DysectErrorCode findSymtabByName(std::string name, Dyninst::Stackwalker::Walker* proc, Dyninst::SymtabAPI::Symtab*& symtab, std::string& libName);
  };
 
  class CodeLocation {
    friend class Location;

    std::map<Dyninst::Address ,std::string*> offsets;

    std::string libName;
    std::map<Dyninst::Stackwalker::Walker*, Dyninst::Address> procLibraries;
  
  public:
    CodeLocation();

    bool addProcLib(Dyninst::Stackwalker::Walker* proc);
    bool getAddrs(Dyninst::Stackwalker::Walker* proc, std::vector<Dyninst::Address>& addrs);

    static bool findSymbol(Dyninst::Stackwalker::Walker* proc, std::string name, std::vector<DysectAPI::CodeLocation*>& symbols);
    static bool findSymbol(Dyninst::SymtabAPI::Symtab* symtab, std::string name, std::string libName, std::vector<DysectAPI::CodeLocation*>& symbols, bool isRegex);

    static bool findFileLine(Dyninst::Stackwalker::Walker* proc, std::string name, int line, std::vector<DysectAPI::CodeLocation*>& locations);
    static bool findFileLine(Dyninst::SymtabAPI::Symtab* symtab, std::string name, int line, std::string libName, std::vector<DysectAPI::CodeLocation*>& locations);

  };

  class DataLocation {

  protected:
    Value::content_t dataType;
    Dyninst::SymtabAPI::Symtab* symtab;

  public:
    enum LocationType {
      LocationUnknown,
      LocationGlobal, //!< Resident in fixed address (lib + offset)
      LocationLocal,  //!< Resident in stack frame
      LocationAbs     //!< Absolute address + size (no symbol information)
    };

    DataLocation(LocationType locationType, Value::content_t dataType, Dyninst::SymtabAPI::Symtab* symtab);

    static bool findVariable( Dyninst::ProcControlAPI::Process::const_ptr process,
                              Dyninst::Stackwalker::Walker* proc, 
                              std::string name, 
                              DysectAPI::DataLocation*& location);

    static bool findVariable( Dyninst::Address libOffset,
                              Dyninst::SymtabAPI::Symtab* symtab, 
                              Dyninst::ProcControlAPI::Process::const_ptr process,
                              std::string name, 
                              DysectAPI::DataLocation*& location);

    static bool getParams( std::vector<std::string>& params,
                           Dyninst::Stackwalker::Walker* proc);

    Value::content_t getDataType();
    LocationType getLocationType();
    bool isStructure();

    virtual DysectErrorCode getValue(Value& val) = 0;
    virtual Dyninst::SymtabAPI::Type* getType() = 0;

    virtual DataLocation* getInnerVariable(Type *newType, int offset) = 0;
  
  protected:
    LocationType locationType;
  };

  class LocalVariableLocation : public DataLocation {
    
    localVar* var;
    int frame;
    std::vector<Stackwalker::Frame> walkedStack;

  public:
    LocalVariableLocation(std::vector<Stackwalker::Frame> &walkedStack, int frame, localVar* var, Dyninst::SymtabAPI::Symtab* symtab);

    DysectErrorCode getValue(Value& val);
    Dyninst::SymtabAPI::Type* getType();

    DataLocation* getInnerVariable(Type *newType, int offset);
  };

  class GlobalVariableLocation : public DataLocation {
    Dyninst::Address offset;
    Dyninst::ProcControlAPI::Process::const_ptr process;
    Dyninst::SymtabAPI::Symbol *sym;
    Dyninst::SymtabAPI::Variable *var;

  public:
    GlobalVariableLocation(Dyninst::Address offset,
                           Dyninst::ProcControlAPI::Process::const_ptr process,
                           Dyninst::SymtabAPI::Symbol* sym,
                           Dyninst::SymtabAPI::Variable *var,
                           Dyninst::SymtabAPI::Symtab* symtab);

    DysectErrorCode getValue(Value& val);
    Dyninst::SymtabAPI::Type* getType();

    DataLocation* getInnerVariable(Type *newType, int offset);
  };
 }

#endif
