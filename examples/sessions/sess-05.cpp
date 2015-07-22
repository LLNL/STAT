#include <DysectAPI.h>

DysectStatus DysectAPI::onProcStart() {

  Probe* p1    = new Probe(Code::location("mpi_ringtopo2.cpp#94"),
                           Domain::world(2000, true),
                           Act::stat());

  ProbeTree::addRoot(p1);

  return DysectOK;
}
