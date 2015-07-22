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

#include <LibDysectAPI.h>
#include <DysectAPI/Err.h>
#include <DysectAPI/Event.h>
#include <DysectAPI/Parser.h>
#include <DysectAPI/Domain.h>
#include <DysectAPI/Probe.h>
#include <DysectAPI/Backend.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace Stackwalker;
using namespace SymtabAPI;
using namespace ProcControlAPI;

map<string, Symtab*> SymbolTable::symtabMap;

Location::Location(string locationExpr, bool pendingEnabled) : locationExpr(locationExpr), pendingEnabled(pendingEnabled), Event() {
  bp = Breakpoint::newBreakpoint();
  bp->setData(this);
}

bool Location::disable() {
  assert(owner != 0);
 
  if(codeLocations.empty()) {
    return true;
  }

  Domain* dom = owner->getDomain();

  ProcessSet::ptr procset;
  
  if(!dom->getAttached(procset)) {
    return DYSECTWARN(false, "Could not get procset from domain");
  }

  if(!procset) {
    return DYSECTWARN(false, "Process set not present");
  }

  return disable(procset);
}

bool Location::disable(ProcessSet::ptr lprocset) {
  assert(owner != 0);
 
  if(codeLocations.empty()) {
    return DYSECTVERBOSE(true, "No code locations");

  }

  if(!lprocset) {
    return DYSECTWARN(false, "Process set not present");
  }

  ProcessSet::iterator procIter = lprocset->begin();
  for(;procIter != lprocset->end(); procIter++) {
    bool wasRunning = false;
    Process::ptr procPtr = *procIter;
    ProcDebug *pDebug;

    Walker* proc = (Walker*)procPtr->getData();

    if (procPtr->allThreadsRunning()) {
      wasRunning = true;
      pDebug = dynamic_cast<ProcDebug *>(proc->getProcessState());
      if (pDebug == NULL)
        return DYSECTWARN(false, "Failed to dynamic_cast ProcDebug pointer");
      if (pDebug->isTerminated())
        return DYSECTWARN(false, "Process is terminated");
      if (pDebug->pause() == false)
        return DYSECTWARN(false, "Failed to pause process");
    }

    vector<DysectAPI::CodeLocation*>::iterator locationIter = codeLocations.begin();
    for(;locationIter != codeLocations.end(); locationIter++) {
      DysectAPI::CodeLocation* location = *locationIter;

      if(!location->addProcLib(proc)) {
        return DYSECTWARN(false, "Symbol not found in process");
      }

      vector<Dyninst::Address> addrs;
      
      if(!location->getAddrs(proc, addrs) || addrs.empty()) {
        return DYSECTWARN(false, "Addresses for symbol could not be determined");
      }

      // Breakpoint locations at hand
      for(int i = 0; i < addrs.size() ; i++) {
        Dyninst::Address addr = addrs[i];
        
        if(!procPtr->rmBreakpoint(addr, bp)) {
          DYSECTVERBOSE(false, "Breakpoint at %lx not removed! %s", addr, ProcControlAPI::getLastErrorMsg());
        }
        else
          DYSECTVERBOSE(false, "Breakpoint at %lx removed for %d!", addr, procPtr->getPid());
      }
    }

    if (wasRunning == true) {
      if (pDebug->resume() == false)
        return DYSECTWARN(false, "Failed to resume process");
    }


  }

  return true;
}

bool Location::enable() {
  assert(owner != 0);
 
  if(codeLocations.empty()) {
    return true;
  }

  Domain* dom = owner->getDomain();

  ProcessSet::ptr procset;
  
  if(!dom->getAttached(procset)) {
    return DYSECTWARN(false, "Could not get procset from domain");
  }

  if(!procset) {
    return DYSECTWARN(false, "Process set not present");
  }

  return enable(procset);
}

bool Location::enable(ProcessSet::ptr lprocset) {
  assert(owner != 0);
 
  if(codeLocations.empty()) {
    return DYSECTVERBOSE(true, "No code locations");

  }

  if(!lprocset) {
    return DYSECTWARN(false, "Process set not present");
  }
  
  ProcessSet::iterator procIter = lprocset->begin();

  AddressSet::ptr addrset = AddressSet::newAddressSet();

  if(lprocset->size() <= 0) {
    return DYSECTINFO(true, "Process set empty!");
  }

  // Find library location for target processes
  for(;procIter != lprocset->end(); procIter++) {
    bool wasRunning = false;
    Process::ptr procPtr = *procIter;
    ProcDebug *pDebug;

    Walker* proc = (Walker*)procPtr->getData();

    if (procPtr->allThreadsRunning()) {
      wasRunning = true;
      pDebug = dynamic_cast<ProcDebug *>(proc->getProcessState());
      if (pDebug == NULL)
        return DYSECTWARN(false, "Failed to dynamic_cast ProcDebug pointer");
      if (pDebug->isTerminated())
        return DYSECTWARN(false, "Process is terminated");
      if (pDebug->pause() == false)
        return DYSECTWARN(false, "Failed to pause process");
    }

    vector<DysectAPI::CodeLocation*>::iterator locationIter = codeLocations.begin();
    for(;locationIter != codeLocations.end(); locationIter++) {
      DysectAPI::CodeLocation* location = *locationIter;

      if(!location->addProcLib(proc)) {
        return DYSECTWARN(false, "Symbol not found in process");
      }

      vector<Dyninst::Address> addrs;
      
      if(!location->getAddrs(proc, addrs) || addrs.empty()) {
        return DYSECTWARN(false, "Addresses for symbol could not be determined");
      }

      // Breakpoint locations at hand
      for(int i = 0; i < addrs.size() ; i++) {
        Dyninst::Address addr = addrs[i];

        if(addr == 0x2aaaaad02e7f)
          addr = 0x2aaaaad02e28;
        
        if(!procPtr->addBreakpoint(addr, bp)) {
          return DYSECTVERBOSE(false, "Breakpoint not installed at %lx: %s", addr, ProcControlAPI::getLastErrorMsg());
        } else {
          DYSECTVERBOSE(true, "Breakpoint installed at %lx for %d", addr, procPtr->getPid());
        }

        //addrset->insert(addr, procPtr);
      }

      //if(addrs.empty()) {
      //  Err::verbose(true, "No addresses");
      //}
    }

    if (wasRunning == true) {
      if (pDebug->resume() == false)
        return DYSECTWARN(false, "Failed to resume process");
    }


  }
  
  //if(addrset->size() <= 0) {
  //  return Err::verbose(true, "Empty address set");
  //}

  
  //if(!lprocset->addBreakpoint(addrset, nbp)) {
  //  return Err::warn(false, "Could not insert breakpoints!");
  //} else {
  //  Err::info(true, "%d breakpoints inserted in %d processes", addrset->size(), lprocset->size());
  //}

  return true;
}

bool Location::prepare() {
  DYSECTVERBOSE(true, "Preparing location");

  if(codeLocations.empty()) {
    if(!resolveExpression()) {
      return DYSECTWARN(false, "Expression '%s' could not be resolved", locationExpr.c_str());
    }
    if(codeLocations.empty()) {
      if(pendingEnabled)
        return DYSECTLOG(false, "No symbols found for expression '%s'", locationExpr.c_str());
      else
        return DYSECTWARN(false, "No symbols found for expression '%s'", locationExpr.c_str());
    }
  }

  return DYSECTVERBOSE(true, "%d symbols found for expression '%s'", codeLocations.size(), locationExpr.c_str());
}

bool Location::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return true;
}

bool Location::resolveExpression() {
  // Location expression parsed:
  //
  // loc_expr   ::= image '#' file '#' line
  //             |  image '#' symbol
  //             |  file '#' line
  //             |  symbol
  // ~symbol for function exit
  
  vector<string> tokens = Parser::tokenize(locationExpr, '#');
  int numTokens = tokens.size();
  Walker* proc = 0;

  if(numTokens > 0) {
    // XXX: Make this local to domain
    WalkerSet* walkerSet = Backend::getWalkerset();

    if(!walkerSet) {
      return DYSECTWARN(false, "Walkerset not present");
    }

    WalkerSet::iterator procIter = walkerSet->begin();
    proc = *procIter;
  }

  if(numTokens == 1) {        // symbol
    string &symbol = tokens[0];

    if(!proc) {
      return DYSECTWARN(false, "Walker could not be retrieved from walkerSet");
    }

    if(!DysectAPI::CodeLocation::findSymbol(proc, symbol, codeLocations)) {
      if (pendingEnabled)
        return DYSECTLOG(true, "No symbols found for symbol %s, pending enabled for future search", symbol.c_str());
      else
        return DYSECTWARN(false, "No symbols found for symbol %s", symbol.c_str());
    }

  } else if(numTokens == 2) { // file '#' line 
    
    int line = atoi(tokens[1].c_str());

    if(line > 0) {  // File '#' line
      string& file = tokens[0];

      if(!DysectAPI::CodeLocation::findFileLine(proc, file, line, codeLocations)) {
        return DYSECTWARN(false, "Location for '%s#%d' not found", file.c_str(), line);
      }
    } else { // Image '#' symbol
      Symtab* symtab;
      string libName;
      string& image = tokens[0];
      string& symbol = tokens[1];

      // XXX: Move this to CodeLocation
      bool isRegex = false;
      int pos = symbol.find_first_of("*?");
      if(pos != string::npos) {
        isRegex = true;
      }

      if(SymbolTable::findSymtabByName(image, proc, symtab, libName) != OK) {
        return DYSECTWARN(false, "Could not find image '%s'", image.c_str());
      }

      if(!CodeLocation::findSymbol(symtab, symbol, libName, codeLocations, isRegex)) {
        return DYSECTWARN(false, "Location for symbol '%s' in image '%s' not found", symbol.c_str(), image.c_str());
      }
    }
  } else if(numTokens == 3) { // image '#' file '#' line
    Symtab* symtab;
    string libName;
    string& image = tokens[0];
    string& file = tokens[1];
    int line = atoi(tokens[2].c_str());

    if(line <= 0) {
      return DYSECTWARN(false, "Line number for format image#file#line must be an integer");
    }

    if(SymbolTable::findSymtabByName(image, proc, symtab, libName) != OK) {
      return DYSECTWARN(false, "Could not find image '%s'", image.c_str());
    }

    if(!CodeLocation::findFileLine(symtab, file, line, libName, codeLocations)) {
      return DYSECTWARN(false, "Location for '%s:%d' in image '%s' not found", file.c_str(), line, image.c_str());
    }

  } else {
    return DYSECTWARN(false, "Cannot resolve location '%s' - unknown format", locationExpr.c_str());
  }

  return true;
}
