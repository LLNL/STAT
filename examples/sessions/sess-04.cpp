#include <DysectAPI.h>

DysectStatus DysectAPI::onProcStart() {

  Probe* p1    = new Probe(Code::location("do_SendOrStall"),
                           Domain::world(2000, true),
                           Act::trace("ranks are @ranks()'"));
//                           Acts(Act::stackTrace(), 
//                                Act::trace("Location is '@location()'")));

  Probe* p2    = new Probe(Code::location("mpi_ringtopo2.cpp#73"),
                           Domain::world(2000, true),
                           Act::trace("@desc(rank)"));
                           //Act::trace("@desc(st->c)"));

  ProbeTree::addRoot(p1);
//  ProbeTree::addRoot(p2);

  return DysectOK;
}
