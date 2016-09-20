#ifndef __FEDUMMYDYNINST_H
#define __FEDUMMYDYNINST_H

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

namespace Dyninst {

typedef int THR_ID;
class Address;

  class Address {
    Address();
  };

namespace Stackwalker {

class Walker;
class WalkerSet;
class LibAddrPair;
class Frame;

  class Walker {
    Walker();
  };

  class WalkerSet {
    WalkerSet();
  };

} //namespace Stackwalker

namespace ProcControlAPI {

class Process;
class Thread;
class ProcessSet;
class ThreadSet;
class Breakpoint;

typedef boost::shared_ptr<Process> Process_ptr;
typedef boost::shared_ptr<const Process> Process_const_ptr;
typedef boost::shared_ptr<Thread> Thread_ptr;
typedef boost::shared_ptr<const Thread> Thread_const_ptr;
typedef boost::shared_ptr<ProcessSet> ProcessSet_ptr;
typedef boost::shared_ptr<const ProcessSet> ProcessSet_const_ptr;
typedef boost::shared_ptr<ThreadSet> ThreadSet_ptr;
typedef boost::shared_ptr<const ThreadSet> ThreadSet_const_ptr;
typedef boost::shared_ptr<Breakpoint> Breakpoint_ptr;
typedef boost::shared_ptr<const Breakpoint> Breakpoint_const_ptr;

  class Process : public boost::enable_shared_from_this<Process> {
  public:
    Process();
    typedef boost::shared_ptr<Process> ptr;
    typedef boost::shared_ptr<const Process> const_ptr;
  };

  class ProcessSet : public boost::enable_shared_from_this<ProcessSet> {
  public:
    ProcessSet();
    typedef boost::shared_ptr<ProcessSet> ptr;
    typedef boost::shared_ptr<const ProcessSet> const_ptr;
  };

  class Thread : public boost::enable_shared_from_this<Thread> {
  public:
    typedef boost::shared_ptr<Thread> ptr;
    typedef boost::shared_ptr<const Thread> const_ptr;
    Thread();
  };

  class ThreadSet : public boost::enable_shared_from_this<ThreadSet> {
  public:
    typedef boost::shared_ptr<ThreadSet> ptr;
    typedef boost::shared_ptr<const ThreadSet> const_ptr;
    ThreadSet();
  };

  class Breakpoint : public boost::enable_shared_from_this<Thread> {
  public:
    typedef boost::shared_ptr<Breakpoint> ptr;
    typedef boost::shared_ptr<const Breakpoint> const_ptr;
    Breakpoint();
  };


} //namespace ProcControlAPI

namespace SymtabAPI {

  class Symtab;
  class Type;
  class localVar;
  class Symbol;
  class Variable;

} //namespace SymtabAPI

} //namespace Dyninst

#endif
