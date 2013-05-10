#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace Stackwalker;
using namespace SymtabAPI;
using namespace ProcControlAPI;

//
// Location operations do not apply for front-end session
//

Location::Location(string locationExpr) : locationExpr(locationExpr) {
}

bool Location::enable() {
  assert(false);
  return true;
}

bool Location::disable() {
  assert(false);
  return true;
}

bool Location::disable(ProcessSet::ptr lprocset) {
  assert(false);
  return true;
}

bool Location::enable(ProcessSet::ptr lprocset) {
  assert(false);
  return true;
}

bool Location::prepare() {
  assert(false);
  return true;
}


bool Location::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return true;
}

bool Location::resolveExpression() {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::findSymbol(Dyninst::Stackwalker::Walker* proc, string name, vector<DysectAPI::CodeLocation*>& symbols) {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::findSymbol(SymtabAPI::Symtab* symtab, string name, string libName, vector<DysectAPI::CodeLocation*>& symbols, bool isRegex) {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::addProcLib(Walker* proc) {
  assert(false);
  return true;
}

bool DysectAPI::CodeLocation::getAddrs(Dyninst::Stackwalker::Walker* proc, std::vector<Dyninst::Address>& addrs) {
  assert(false);
  return true;
}

DysectAPI::CodeLocation::CodeLocation() {
}
