#include <LibDysectAPI.h>
#include <stdio.h>

DysectStatus DysectAPI::onProcStart() {
  
  Probe* foo    = new Probe(Code::location("WeightList::sum"),
                            Domain::group("0-5", 1500));
  
  //Probe* bar    = new Probe(Code::location("bar"),
  //                          Domain::inherit());

  //foo->link(bar);

  ProbeTree::addRoot(foo); 

  return DysectOK;
}
