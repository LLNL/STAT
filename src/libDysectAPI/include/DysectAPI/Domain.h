#ifndef __DOMAIN_H
#define __DOMAIN_H

namespace DysectAPI {
  class World;
  class Group;
  class Domain;
  class Inherit;
  
  namespace Wait {
    enum WaitTime {
      inherit = - 2,
      inf = -1,
      NoWait = 0
    };
  }

  typedef int tag_t;

	class Domain {
    friend class Probe;
    friend class Backend;
    friend class Frontend;

  public:
    enum DomainType {
      UnknownDomain,
      InheritedDomain,
      GroupDomain,
      WorldDomain
    };

  protected:
    static MRN::Network *network;
    static int upstreamFilterId;
    
    static MPIR_PROCDESC_EXT *processTable;
    static long processTableSize;

    // Only available on front-end
    static std::map<int, IntList_t *>* mrnetRankToMpiRanksMap;

    // Only available on back-end
    static Dyninst::ProcControlAPI::ProcessSet::ptr allProcesses;
    static Dyninst::Stackwalker::WalkerSet* allWalkers;
    static std::map<int, Dyninst::ProcControlAPI::Process::ptr> *mpiRankToProcessMap;

    static int domainIdCounter; //!< System-wide domain id counter. Used when generating per-domain tags.
    static std::map<tag_t, Domain*> domainMap;
    static std::map<int, Domain*>   fdMap;
    static fd_set fdSet;
    static int maxFd;

    static bool isTag(tag_t tag, tag_t type);

    class Probe* owner;

    long waitTime;
    int  id;
    bool blocking;

    MRN::Communicator *comm;
    MRN::Stream *stream;
    
    tag_t initTag;
    tag_t initAckTag;
    tag_t exitTag;
    tag_t exitAckTag;
    tag_t errorTag;
    tag_t continueTag;
    tag_t statusTag;
    tag_t probeEnabledTag;
    tag_t probeNotifyTag;

    DysectErrorCode createStreamGeneric();

    Domain();
    Domain(long waitTime, bool lblocking, DomainType domainType);
    ~Domain();

  public:

    static tag_t tagToId(tag_t tag);
    static bool addToMap(Domain* dom);
    static std::map<tag_t, Domain*>& getDomainMap();
    static bool getDomainFromId(Domain*& dom, tag_t id);
    static bool getDomainFromTag(Domain*& dom, tag_t tag);

		static Domain* world(long waitTime = Wait::inf, bool lblocking = true);
		static Domain* group(std::string groupExpr, long waitTime = Wait::inf, bool lblocking = true);
    static Domain* inherit(long waitTime = Wait::inherit, bool lblocking = true);

    static DysectErrorCode createFdSet();
    static std::vector<Domain*> getFdsFromSet(fd_set set);
    static fd_set getFdSet();
    static int getMaxFd();

    static int getTotalNumProcs();

    static bool setFEContext(struct DysectFEContext_t* context);
    static bool setBEContext(struct DysectBEContext_t* context);

    enum predefinedTags {
      initTagId         = 1,
      initAckTagId      = 2,
      exitTagId         = 3,
      exitAckTagId      = 4,
      errorTagId        = 5,
      continueTagId     = 6,
      statusTagId       = 7,
      probeEnabledTagId = 8,
      probeNotifyTagId  = 9
    };

    static bool isInitTag(tag_t tag)          { return isTag(tag, initTagId); }
    static bool isInitAckTag(tag_t tag)       { return isTag(tag, initAckTagId); }
    static bool isExitTag(tag_t tag)          { return isTag(tag, exitTagId); }
    static bool isExitAckTag(tag_t tag)       { return isTag(tag, exitAckTagId); }
    static bool isErrorTag(tag_t tag)         { return isTag(tag, errorTagId); }
    static bool isContinueTag(tag_t tag)      { return isTag(tag, continueTagId); }
    static bool isStatusTag(tag_t tag)        { return isTag(tag, statusTagId); }
    static bool isProbeEnabledTag(tag_t tag)  { return isTag(tag, probeEnabledTagId); }
    static bool isProbeNotifyTag(tag_t tag)   { return isTag(tag, probeNotifyTagId); }

    tag_t getInitTag()          { return initTag; }
    tag_t getInitAckTag()       { return initAckTag; }
    tag_t getExitTag()          { return exitTag; }
    tag_t getExitAckTag()       { return exitAckTag; }
    tag_t getErrorTag()         { return errorTag; }
    tag_t getContinueTag()      { return continueTag; }
    tag_t getStatusTag()        { return statusTag; }
    tag_t getProbeEnabledTag()  { return probeEnabledTag; }
    tag_t getProbeNotifyTag()   { return probeNotifyTag; }

    tag_t getId();

    MRN::Stream* getStream();
    void setStream(MRN::Stream* stream);

    bool isWorld();
    bool isGroup();
    bool isInherited();

    Probe* getOwner() { return owner; }
    void setOwner(Probe* probe) { owner = probe; }

    bool isBlocking();

    long getWaitTime();
    int getDataEventDescriptor();

    bool sendContinue();
    DysectErrorCode broadcastStreamInit();

    virtual bool anyTargetsAttached() = 0;
    virtual bool getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset) = 0;

    /** When probe tree is traversed, this method is called for statically creating
        all needed streams prior to actual session run. */
    virtual DysectErrorCode createStream() = 0;

    /** When backends connect streams to domains, ids and tags must be generated
        exactly as at front-end probe-tree.
        This traversal does not invoke any MRNet calls */
    virtual DysectErrorCode prepareStream() = 0;

  protected:
    DomainType domainType;
	};

  /** Domain for all processes.
      Stream is created with MRNet's default world communicator */
  class World : public Domain {
  public:
    World(long waitTime = Wait::inf, bool lblocking = false);

    DysectErrorCode createStream();
    DysectErrorCode prepareStream();

    bool anyTargetsAttached();
    bool getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset);
  };

  /** Inherits domain from parent probe (parent of owning probe)
      Resolving the actual domain can first be made when tree is traversed by stream creation */
  class Inherit : public Domain {
    bool prepared;
    bool tblocking;

  public:
    Inherit(long waitTime = Wait::inherit, bool lblocking = false);

    bool anyTargetsAttached();

    DysectErrorCode createStream();
    DysectErrorCode prepareStream();

    DysectErrorCode copyDomainFromParent(Domain*& retDom);

    bool getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset);
  };
};

#endif
