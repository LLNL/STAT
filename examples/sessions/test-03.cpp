#include <DysectAPI.h>

DysectStatus DysectAPI::onProcStart() {

  // test various domains

  Probe *entry  = new Probe(Code::location("do_SendOrStall"),
                            Domain::group("0-4", true),
                            Act::trace("Entered do_SendOrStall(), location = @location()"));

  Probe *exit  = new Probe(Code::location("~do_SendOrStall"),
                            Domain::world(2002, true),
                            Act::trace("Exiting do_SendOrStall(), location = @location()"));

  // all tasks should trigger this simultaneously
  Probe *codeBreakpoint  = new Probe(Code::location("dysect_test.cpp#81"),
                            Domain::world(2001, true),
                            Act::trace("Breakpoint in do_SendOrStall(), location = @location(), rank = @desc(rank)"));

  // despite being added after the above BP, this will trigger first since the above BP
  // requires all tasks to participate. Furthermore, task 5 will trigger this first since
  // tasks 0-4 must break on the entry probe
  Probe *codeBreakpoint2 = new Probe(Code::location("dysect_test.cpp#81"),
                            Domain::group("0,2-3,5", true),
                            Act::trace("Breakpoint 2 in do_SendOrStall(), location = @location(), rank = @desc(rank)"));

  /* Within 500 ms and call frame has not been left */
  Probe *timer  = new Probe(Event::And(Time::within(5000), Event::Not(Code::location("~do_SendOrStall"))),
                            Domain::inherit(3001, true),
                            Acts(Act::stackTrace(),
                                 Act::trace("Took more than 5000ms to return from do_SendOrStall(), rank = @desc(do_SendOrStall:rank) location = @location()")));

  // task 1 will be delayed in triggering this probe
  Probe *endOfFunction  = new Probe(Code::location("dysect_test.cpp#107"),
                            Domain::inherit(1003, true),
                            Act::trace("inherited domain @desc(rank) reached location = @location()"));

  // only task 1 will print this since the parent probe is bound to ranks 0-4
  Probe *afterBarrier = new Probe(Code::location("dysect_test.cpp#60"),
                            Domain::group("0,5,6", 1001, true),
                            Acts(Act::stackTrace(),
                                 Act::trace("@desc(rank) reached location = @location()")));

  /* Event chain */
  entry->link(timer);
  entry->link(endOfFunction);
  entry->link(afterBarrier);

  /* Setup probe trees */
  ProbeTree::addRoot(entry);
  ProbeTree::addRoot(codeBreakpoint);
  ProbeTree::addRoot(codeBreakpoint2);
  ProbeTree::addRoot(exit);

  return DysectOK;
}

