#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;

vector<Probe*> ProbeTree::roots;

bool ProbeTree::addRoot(Probe* probe) {
  ProbeTree::roots.push_back(probe);

  return true; 
}

vector<Probe*>& ProbeTree::getRoots() {
  return roots;
}
