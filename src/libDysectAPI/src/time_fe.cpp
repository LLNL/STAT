#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ProcControlAPI;
using namespace Stackwalker;

using namespace Dyninst;
using namespace Stackwalker;

bool Time::enable() {
  return true;
}

bool Time::enable(ProcessSet::ptr lprocset) {
  return true;
}

bool Time::disable() {
  return true;
}

bool Time::disable(ProcessSet::ptr lprocset) {
  return true;
}

bool Time::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return true;
}
