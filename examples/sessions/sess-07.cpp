#include <LibDysectAPI.h>

DysectStatus DysectAPI::onProcStart() {
  int value = 10;
//  long value2 = 10;
  string libraryPath = "/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test/lib/libdepositcorewrap.so", variableName = "globalMpiRank", functionName = "depositcore_wrap_signal_handler";

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
                                Act::loadLibrary(libraryPath),
                                //Act::writeModuleVariable(libraryPath, variableName, (void *)&value, sizeof(int))));
                                Act::writeModuleVariable(libraryPath, variableName, (void *)&value, sizeof(int)),
                                Act::irpc(libraryPath, functionName, (void *)&value, sizeof(int))));
                                //Act::signal(SIGUSR1)));

  Probe *p4    = new Probe(Time::within(0),
                           Domain::group("7"),
                           Act::signal(SIGUSR1));

//  p3->link(p4);

  ProbeTree::addRoot(p1);
//  ProbeTree::addRoot(p2);
  ProbeTree::addRoot(p3);
  return DysectOK;
}
