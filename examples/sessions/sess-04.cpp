#include <DysectAPI.h>

DysectStatus DysectAPI::onProcStart(int argc, char **argv) {

  Probe* p1    = new Probe(Code::location("do_SendOrStall"),
                           Domain::world(2000, true),
                           Acts(Act::stackTrace(),
                                Act::trace("Location is '@location()'")));

  Probe* p2    = new Probe(Code::location("mpi_ringtopo2.cpp#77"),
                           Domain::world(2000, true),
                           Acts(Act::trace("@desc(st->a)"),
                                Act::trace("@desc(st->b)")));

  ProbeTree::addRoot(p1);
  ProbeTree::addRoot(p2);

  return DysectOK;
}
