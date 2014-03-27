#ifndef __PROBE_TREE_H
#define __PROBE_TREE_H

namespace DysectAPI {
  class Probe;

  class ProbeTree {
    static std::vector<Probe*> roots;

  public:
    static bool addRoot(Probe* probe);
    static std::vector<Probe*>& getRoots();
  };
}

#endif
