#include <LibDysectAPI.h>

DysectStatus DysectAPI::onProcStart() {
  Probe* p1    = new Probe(Code::location("mpi_ringtopo2.cpp#97"),
                           Domain::group("3-5,7"),
                           Act::totalview());

  Probe* p2    = new Probe(Code::location("mpi_ringtopo2.cpp#76"),
                           Domain::group("3-5,7"),
                           Acts(Act::trace("Location is '@location()'"),
                                Act::depositCore()));
  
  ProbeTree::addRoot(p1);
  ProbeTree::addRoot(p2);
  return DysectOK;
}
