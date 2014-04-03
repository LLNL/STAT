#include <DysectAPI.h>

DysectStatus DysectAPI::onProcStart() {
  Probe* entry    = new Probe(Code::location("do_SendOrStall"),
                              Domain::world(1000), Act::trace("Function @function() entry"));


  ProbeTree::addRoot(entry); 

  return DysectOK;
}
