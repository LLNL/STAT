#include <DysectAPI.h>

DysectStatus DysectAPI::defaultProbes() {

  // XXX: Check if probes, handling these events, already is in place

  Probe* exit = new Probe(Async::exit(), Domain::world(), Acts(Act::trace("Application exited"), Act::detach()));
  Probe* segv = new Probe(Async::signal(SIGSEGV), Domain::world(500), Act::trace("Memory violation - segmentation fault in function @function()"));
  Probe* sigbus = new Probe(Async::signal(SIGBUS), Domain::world(500), Act::trace("Memory violation - bus error in function @function()"));
  Probe* fpe = new Probe(Async::signal(SIGFPE), Domain::world(500), Act::trace("Floating point exception in function @function()"));

  ProbeTree::addRoot(exit);
  ProbeTree::addRoot(segv);
  ProbeTree::addRoot(sigbus);
  ProbeTree::addRoot(fpe);

  return DysectOK;
}
