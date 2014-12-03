#include <LibDysectAPI.h>

DysectStatus DysectAPI::onProcStart() {
  int value = 10;
  string libName = "/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test/lib/libdepositcorewrap.so", varName = "globalMpiRank";

  Probe* p1    = new Probe(Code::location("mpi_ringtopo2.cpp#97"),
                           Domain::group("3-5,7"),
                           Act::totalview());

  Probe* p2    = new Probe(Code::location("mpi_ringtopo2.cpp#76"),
                           Domain::group("3-5,7"),
                           Acts(Act::trace("Location is '@location()'"),
                                Act::depositCore()));

  Probe* p3    = new Probe(Code::location("mpi_ringtopo2.cpp#76"),
                           Domain::group("3-5,7"),
                           Acts(Act::trace("Location is '@location()'"),
                                Act::loadLibrary(libName),
                                Act::writeVariable(varName, libName, (void *)&value, sizeof(int))));
                                //Act::writeVariable(varName, libName, (void *)&value, sizeof(int)),
                                //Act::signal(SIGUSR1)));

  Probe *p4    = new Probe(Time::within(0),
                           Domain::group("7"),
                           Act::signal(SIGUSR1));

  p3->link(p4);

  ProbeTree::addRoot(p1);
//  ProbeTree::addRoot(p2);
  ProbeTree::addRoot(p3);
  return DysectOK;
}
