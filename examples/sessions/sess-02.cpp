#include <DysectAPI.h>
#include <stdio.h>

// Single entry for debug daemon
DysectStatus DysectAPI::onProcStart(int argc, char **argv) {
  
  /* Probe creation */
  Probe* foo    = new Probe(Code::location("foo"),
                            Domain::world(1000),
                            Act::stat(AllProcs));
  
  Probe* square    = new Probe(Code::location("square<int>"),
                               Domain::inherit(2000),
                               Act::trace("At square"));

  /* Event chain */
  foo->link(square);

  /* Setup probe tree */
  ProbeTree::addRoot(foo); 

  return DysectOK;
}
