#include <DysectAPI.h>

DysectStatus DysectAPI::defaultProbes() {

  // XXX: Check if probes, handling these events, already is in place

  Probe* exit = new Probe(Async::exit(), Domain::world(500), Acts(Act::trace("Process exited"), Act::detach()));
  Probe* segv = new Probe(Async::signal(SIGSEGV), Domain::world(500, true), Acts(Act::trace("Memory violation - segmentation fault"), Act::stackTrace(), Act::stat()));//, Act::detach()));
  Probe* sigbus = new Probe(Async::signal(SIGBUS), Domain::world(500), Act::trace("Memory violation - bus error in function @function()"));
  Probe* fpe = new Probe(Async::signal(SIGFPE), Domain::world(500), Act::trace("Floating point exception in function @function()"));

  ProbeTree::addRoot(fpe);
  ProbeTree::addRoot(exit);
  ProbeTree::addRoot(segv);
  ProbeTree::addRoot(sigbus);

  return DysectOK;
}
