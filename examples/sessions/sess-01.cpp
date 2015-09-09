#include <DysectAPI.h>
#include <stdio.h>

// Single entry for debug daemon
DysectStatus DysectAPI::onProcStart(int argc, char **argv) {
  
  /* Probe creation */
  Probe* entry  = new Probe(Code::location("entry(foo)"),
                            Local::eval("argc >= 5"),
                            Domain::group("12,25,65-70", Wait::inf));

  /* Within 500 ms and call frame has not been left */
  Probe* timer  = new Probe(Event::And(Time::within(500), Event::Not(Async::leaveFrame())),
                            Domain::group("..", 400), /* Wait 400 ms for other to arrive */
                            Action::trace("Took more than 500ms to return from foo()"));
  
  /* Event chain */
  entry->link(timer);

  /* Setup probe tree */
  entry->setup();

  return DysectOK;
}
