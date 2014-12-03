/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __ACTION_H
#define __ACTION_H

namespace DysectAPI {
  class Act;
  class Probe;
  class Backend;

  enum AggScope {
    SatisfyingProcs = 1,
    InvSatisfyingProcs = 2,
    AllProcs = 3
  };

  class Act {
    friend class Probe;

    static int aggregateIdCounter;
    //static std::map<int, Act*> aggregateMap;

  protected:
    AggScope lscope;

    typedef enum aggCategory {
      unknownCategory,
      userAction,
      conditional
    } aggCategory;

    typedef enum aggType {
      unknownAggType = 0,
      traceType = 1,
      statType = 2,
      detachAllType = 3,
      stackTraceType = 4,
      detachType = 5,
      totalviewType = 6,
      depositCoreType = 7,
      loadLibraryType = 8,
      writeModuleVariableType = 9,
      signalType = 10,
      irpcType = 11,
    } aggType;

    aggType type;
    aggCategory category;
    int id;
    int count;
    bool actionPending;

    bool getFromByteArray(std::vector<Act*>& aggregates);

    Probe* owner;

    Act();

  public:
    static Act* trace(std::string str);
    static Act* totalview();
    static Act* depositCore();
    static Act* signal(int sigNum);
    static Act* loadLibrary(std::string library);
    static Act* irpc(std::string libraryPath, std::string functionNAme, void *value, int size);
    static Act* writeModuleVariable(std::string libraryPath, std::string variableName, void *value, int size);
    static Act* stat(AggScope scope = SatisfyingProcs, int traces = 5, int frequency = 300, bool threads = false);
    static Act* detachAll(AggScope scope = AllProcs);
    static Act* detach();
    static Act* stackTrace();

    int getId() { return id; }
    virtual bool prepare() = 0;
    virtual bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                          Dyninst::ProcControlAPI::Thread::const_ptr thread) = 0;
    virtual bool finishBE(struct packet*& p, int& len) = 0;
    virtual bool finishFE(int count) = 0;
  };

  std::vector<Act*> Acts(Act* act1, Act* act2 = 0, Act* act3 = 0, Act* act4 = 0, Act* act5 = 0, Act* act6 = 0, Act* act7 = 0, Act* act8 = 0);


  class LoadLibrary : public Act {
    std::vector<Dyninst::ProcControlAPI::Process::ptr> triggeredProcs;
    std::string library;

    public:
    LoadLibrary(std::string library);
    bool prepare();
    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);
    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };


  class WriteModuleVariable : public Act {
    std::vector<Dyninst::ProcControlAPI::Process::ptr> triggeredProcs;
    std::string variableName;
    std::string libraryPath;
    void *value;
    int size;

    public:
    WriteModuleVariable(std::string libraryPath, std::string variableName, void *value, int size);
    bool prepare();
    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);
    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };


  class Irpc : public Act {
    std::vector<Dyninst::ProcControlAPI::Process::ptr> triggeredProcs;
    std::string functionName;
    std::string libraryPath;
    void *value;
    int size;

    public:
    Irpc(std::string libraryPath, std::string functionName, void *value, int size);
    bool prepare();
    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);
    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };


  class Signal : public Act {
    std::vector<Dyninst::ProcControlAPI::Process::ptr> triggeredProcs;
    int sigNum;

    public:
    Signal(int sigNum);
    bool prepare();
    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);
    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };


  class DepositCore : public Act {
    std::vector<AggregateFunction*> aggregates;
    bool findAggregates();
    std::map<int, Dyninst::ProcControlAPI::Process::ptr> triggeredProcs;

    public:
    DepositCore();

    bool prepare();

    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };


  class Totalview : public Act {
    std::vector<AggregateFunction*> aggregates;
    bool findAggregates();

    public:
    Totalview();

    bool prepare();

    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };

  class Stat : public Act {
    int traces;
    int frequency;
    bool threads;

    public:
    Stat(AggScope scope, int traces, int frequency, bool threads);

    bool prepare();

    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };


  class Trace : public Act {
    std::string str;
    std::vector<AggregateFunction*> aggregates;
    std::vector<std::pair<bool, std::string> > strParts;

    bool findAggregates();

  public:
    Trace(std::string str);

    bool prepare();

    bool collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };

  class StackTrace : public Act {
    std::string str;
    StackTraces* traces;

  public:
    StackTrace();

    bool prepare();

    bool collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                 Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };

  class DetachAll : public Act {
  public:
    DetachAll(AggScope scope);

    bool prepare();

    bool collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                 Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };

  class Detach : public Act {
  public:
    Detach();

    bool prepare();

    bool collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                 Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  private:
    Dyninst::ProcControlAPI::ProcessSet::ptr detachProcs;

  };
}

#endif
