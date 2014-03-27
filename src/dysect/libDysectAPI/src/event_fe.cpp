#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ProcControlAPI;
using namespace Stackwalker;

using namespace Dyninst;
using namespace Stackwalker;

bool Async::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return true;
}

bool Async::enable() {
  return true;
}

// XXX: Process scope for signal handlers
bool Async::enable(ProcessSet::ptr lprocset) {
  return true;
}

bool Async::disable() {
  return true;
}

bool Async::disable(ProcessSet::ptr lprocset) {
  return true;
}

bool Async::prepare() {
  return true;
}
