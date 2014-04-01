#include <LibDysectAPI.h>

DysectStatus DysectAPI::onProcStart() {

  Probe* p1    = new Probe(Code::location("mpi_ringtopo2.cpp#94"),
                           //Domain::world(2000, true),
                           Domain::group("1,4,5", 2000),
                           //Domain::group("4,5"),
                           Acts(Act::stackTrace(), 
                                Act::trace("@desc(rank): Location is @function():@location()"),
                                Act::trace("Function is '@function()'"),
                                Act::trace("Location is '@location()'"),
                                Act::trace("Rank is '@desc(rank)'"),
                                Act::stat()));
  Probe* p1b   = new Probe(Time::within(0),
                           Domain::group("4,5", 1000),
                           Acts(Act::trace("Location is @location()'"),
                                Act::detach()));
  p1->link(p1b);
  Probe* p2    = new Probe(Code::location("mpi_ringtopo2.cpp#73"),
                           Domain::world(),
                           Acts(Act::stackTrace(), 
                                Act::trace("Location is '@location()'"),
                                Act::stat()));
  
  Probe* p3    = new Probe(Code::location("mpi_ringtopo2.cpp#60"),
                           Domain::world(5000, true),
                           Act::stat());

  ProbeTree::addRoot(p1);
  ProbeTree::addRoot(p2);
  ProbeTree::addRoot(p3);

  return DysectOK;
}
