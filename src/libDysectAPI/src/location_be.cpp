#include <DysectAPI.h>
#include <DysectAPI/Backend.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace Stackwalker;
using namespace SymtabAPI;
using namespace ProcControlAPI;

map<string, Symtab*> SymbolTable::symtabMap;

Location::Location(string locationExpr) : locationExpr(locationExpr), Event() {
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
    return Err::warn(false, "Could not get procset from domain");
  }

  if(!procset) {
    return Err::warn(false, "Process set not present");
  }

  return disable(procset);
}

bool Location::disable(ProcessSet::ptr lprocset) {
  assert(owner != 0);
 
  if(codeLocations.empty()) {
    return Err::verbose(true, "No code locations");

  }

  if(!lprocset) {
    return Err::warn(false, "Process set not present");
  }

  ProcessSet::iterator procIter = lprocset->begin();
  for(;procIter != lprocset->end(); procIter++) {
    Process::ptr procPtr = *procIter;

    Walker* proc = (Walker*)procPtr->getData();

    vector<DysectAPI::CodeLocation*>::iterator locationIter = codeLocations.begin();
    for(;locationIter != codeLocations.end(); locationIter++) {
      DysectAPI::CodeLocation* location = *locationIter;

      if(!location->addProcLib(proc)) {
        return Err::warn(false, "Symbol not found in process");
      }

      vector<Dyninst::Address> addrs;
      
      if(!location->getAddrs(proc, addrs) || addrs.empty()) {
        return Err::warn(false, "Addresses for symbol could not be determined");
      }

      // Breakpoint locations at hand
      for(int i = 0; i < addrs.size() ; i++) {
        Dyninst::Address addr = addrs[i];
        
        if(!procPtr->rmBreakpoint(addr, bp)) {
          Err::verbose(false, "Breakpoint not removed! %s", ProcControlAPI::getLastErrorMsg());
        }
      }
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
    return Err::warn(false, "Could not get procset from domain");
  }

  if(!procset) {
    return Err::warn(false, "Process set not present");
  }

  return enable(procset);
}

bool Location::enable(ProcessSet::ptr lprocset) {
  assert(owner != 0);
 
  if(codeLocations.empty()) {
    return Err::verbose(true, "No code locations");

  }

  if(!lprocset) {
    return Err::warn(false, "Process set not present");
  }
  
  ProcessSet::iterator procIter = lprocset->begin();

  AddressSet::ptr addrset = AddressSet::newAddressSet();

  if(lprocset->size() <= 0) {
    return Err::info(true, "Process set empty!");
  }

  // Find library location for target processes
  for(;procIter != lprocset->end(); procIter++) {
    Process::ptr procPtr = *procIter;

    Walker* proc = (Walker*)procPtr->getData();

    vector<DysectAPI::CodeLocation*>::iterator locationIter = codeLocations.begin();
    for(;locationIter != codeLocations.end(); locationIter++) {
      DysectAPI::CodeLocation* location = *locationIter;

      if(!location->addProcLib(proc)) {
        return Err::warn(false, "Symbol not found in process");
      }

      vector<Dyninst::Address> addrs;
      
      if(!location->getAddrs(proc, addrs) || addrs.empty()) {
        return Err::warn(false, "Addresses for symbol could not be determined");
      }

      // Breakpoint locations at hand
      for(int i = 0; i < addrs.size() ; i++) {
        Dyninst::Address addr = addrs[i];
        
        if(!procPtr->addBreakpoint(addr, bp)) {
          return Err::verbose(false, "Breakpoint not installed: %s", ProcControlAPI::getLastErrorMsg());
        } else {
          Err::verbose(true, "Breakpoint installed at %lx", addr);
        }

        //addrset->insert(addr, procPtr);
      }

      //if(addrs.empty()) {
      //  Err::verbose(true, "No addresses");
      //}
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
  Err::verbose(true, "Preparing location");

  if(codeLocations.empty()) {
    if(!resolveExpression()) {
      return Err::warn(false, "Expression '%s' could not be resolved", locationExpr.c_str());
    }

    if(codeLocations.empty()) {
      return Err::warn(false, "No symbols found for expression '%s'", locationExpr.c_str());
    }
  }

  return Err::verbose(true, "%d symbols found for expression '%s'", codeLocations.size(), locationExpr.c_str());
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
  
  vector<string> tokens = Parser::tokenize(locationExpr, '#');
  int numTokens = tokens.size();
  Walker* proc = 0;

  if(numTokens > 0) {
    // XXX: Make this local to domain
    WalkerSet* walkerSet = Backend::getWalkerset();

    if(!walkerSet) {
      return Err::warn(false, "Walkerset not present");
    }

    WalkerSet::iterator procIter = walkerSet->begin();
    proc = *procIter;
  }

  if(numTokens == 1) {        // symbol
    string &symbol = tokens[0];

    if(!proc) {
      return Err::warn(false, "Walker could not be retrieved from walkerSet");
    }

    if(!DysectAPI::CodeLocation::findSymbol(proc, symbol, codeLocations)) {
      return Err::warn(false, "No symbols found for symbol");
    }

  } else if(numTokens == 2) { // file '#' line 
    
    int line = atoi(tokens[1].c_str());

    if(line > 0) {  // File '#' line
      string& file = tokens[0];

      if(!DysectAPI::CodeLocation::findFileLine(proc, file, line, codeLocations)) {
        return Err::warn(false, "Location for '%s#%d' not found", file.c_str(), line);
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
        return Err::warn(false, "Could not find image '%s'", image.c_str());
      }

      if(!CodeLocation::findSymbol(symtab, symbol, libName, codeLocations, isRegex)) {
        return Err::warn(false, "Location for symbol '%s' in image '%s' not found", symbol.c_str(), image.c_str());
      }
    }
  } else if(numTokens == 3) { // image '#' file '#' line
    Symtab* symtab;
    string libName;
    string& image = tokens[0];
    string& file = tokens[1];
    int line = atoi(tokens[2].c_str());

    if(line <= 0) {
      return Err::warn(false, "Line number for format image#file#line must be an integer");
    }

    if(SymbolTable::findSymtabByName(image, proc, symtab, libName) != OK) {
      return Err::warn(false, "Could not find image '%s'", image.c_str());
    }

    if(!CodeLocation::findFileLine(symtab, file, line, libName, codeLocations)) {
      return Err::warn(false, "Location for '%s:%d' in image '%s' not found", file.c_str(), line, image.c_str());
    }

  } else {
    return Err::warn(false, "Cannot resolve location '%s' - unknown format", locationExpr.c_str());
  }

  return true;
}
