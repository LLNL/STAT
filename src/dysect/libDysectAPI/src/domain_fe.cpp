#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace Stackwalker;
using namespace MRN;


bool World::getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset) {
  assert(!"Attached processes cannot be fetched on frontend");
  return false;
}
