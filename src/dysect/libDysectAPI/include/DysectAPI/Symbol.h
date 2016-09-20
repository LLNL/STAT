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

#ifndef __SYMBOL_H
#define __SYMBOL_H

#include <string>
#include <vector>
#include <map>

#include <DysectAPI/Aggregates/Aggregate.h>
#include <DysectAPI/Aggregates/Data.h>

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

    virtual DataLocation* getInnerVariable(Dyninst::SymtabAPI::Type *newType, int offset) = 0;
  
  protected:
    LocationType locationType;
  };

  class LocalVariableLocation : public DataLocation {
    
    Dyninst::SymtabAPI::localVar* var;
    int frame;
    std::vector<Dyninst::Stackwalker::Frame> walkedStack;

  public:
    LocalVariableLocation(std::vector<Dyninst::Stackwalker::Frame> &walkedStack, int frame, Dyninst::SymtabAPI::localVar* var, Dyninst::SymtabAPI::Symtab* symtab);

    DysectErrorCode getValue(Value& val);
    Dyninst::SymtabAPI::Type* getType();

    DataLocation* getInnerVariable(Dyninst::SymtabAPI::Type *newType, int offset);
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

    DataLocation* getInnerVariable(Dyninst::SymtabAPI::Type *newType, int offset);
  };
 }

#endif
