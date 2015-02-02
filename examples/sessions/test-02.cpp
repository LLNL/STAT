#include <LibDysectAPI.h>
#include <stdio.h>

DysectStatus DysectAPI::onProcStart() {

  // Test various code locations, the Time event, and probe chaining
  //
  Probe *entry  = new Probe(Code::location("do_SendOrStall"),
                            Domain::world(2003, true),
                            Act::trace("Entered do_SendOrStall(), location = @location()"));

  Probe *codeBreakpoint  = new Probe(Code::location("dysect_test.cpp#81"),
                            Domain::world(2001, true),
                            Act::trace("Breakpoint in do_SendOrStall(), location = @location()"));

  Probe *exit  = new Probe(Code::location("~do_SendOrStall"),
                            Domain::world(2002, true),
                            Act::trace("Exiting do_SendOrStall(), location = @location()"));

  /* Within 500 ms and call frame has not been left */
//  Probe* slowFunctionTimer  = new Probe(Event::And(Time::within(5000), Event::Not(Async::leaveFrame())), //leaveFrame currently mapped to onCrash...
  Probe *slowFunctionTimer  = new Probe(Event::And(Time::within(5000), Event::Not(Code::location("~do_SendOrStall"))),
                            Domain::world(1001, true),
                            Acts(Act::stackTrace(),
                                 Act::trace("Took more than 5000ms to return from do_SendOrStall(), location = @location()")));

  Probe *endOfFunctionBreakpoint  = new Probe(Code::location("dysect_test.cpp#107"),
                            Domain::world(1002, true),
                            Act::trace("Rank = @desc(rank) reached location = @location()"));

  Probe *leaveFrame  = new Probe(Async::leaveFrame(), //leaveFrame currently mapped to onCrash...
                            Domain::world(1003, true),
                            Act::trace("Rank = @desc(rank) leaving frame do_SendOrStall(), location = @location()"));

  /* Event chain */
  entry->link(slowFunctionTimer);
  entry->link(endOfFunctionBreakpoint);
  entry->link(leaveFrame);

  /* Setup probe trees */
  ProbeTree::addRoot(entry);
  ProbeTree::addRoot(codeBreakpoint);
  ProbeTree::addRoot(exit);

  return DysectOK;
}

