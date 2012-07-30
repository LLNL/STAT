AC_DEFUN([X_AC_MRNET], [
  AC_LANG_PUSH(C++)
  AC_ARG_WITH(mrnet,
    [AS_HELP_STRING([--with-mrnet=prefix],
      [Add the compile and link search paths for mrnet]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
      MRNETPREFIX="${withval}"
      LDFLAGS="$LDFLAGS -L${withval}/lib"
      RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"
    ],
    [CXXFLAGS="$CXXFLAGS"
      MRNETPREFIX=""
    ]
  )
  AC_CHECK_HEADER(mrnet/MRNet.h,
    [],
    [AC_MSG_ERROR([mrnet/MRNet.h is required.  Specify mrnet prefix with --with-mrnet])],
    AC_INCLUDES_DEFAULT
  )
  AC_MSG_CHECKING([Checking MRNet Version])
  mrnet_vers=-1
  AC_COMPILE_IFELSE([#include "mrnet/MRNet.h"
    using namespace MRN;
    using namespace std;
    int main()
    {
      unsigned long bufLength;
      DataType type;
      DataElement pkt;
      pkt.get_array(&type, &bufLength);
    }],
    [AC_DEFINE([MRNET40], [], [MRNet 4.0])
      AC_DEFINE([MRNET31], [], [MRNet 3.1])
      AC_DEFINE([MRNET3], [], [MRNet 3.X]) 
      AC_DEFINE([MRNET22], [], [MRNet 2.2]) 
      AC_DEFINE([MRNET2], [], [MRNet 2.X])
      mrnet_vers=4.0
    ]
  )
  if test $mrnet_vers = -1; then
    AC_COMPILE_IFELSE([#include "mrnet/MRNet.h"
      using namespace MRN;
      using namespace std;
      int main()
      {
        Network *net;
        vector<const char *> f;
        vector<int> fid;
        net->load_FilterFuncs(NULL, f, fid);
      }],
      [AC_DEFINE([MRNET31], [], [MRNet 3.1])
        AC_DEFINE([MRNET3], [], [MRNet 3.X]) 
        AC_DEFINE([MRNET22], [], [MRNet 2.2]) 
        AC_DEFINE([MRNET2], [], [MRNet 2.X])
        mrnet_vers=3.1
      ]
    )
  fi
  if test $mrnet_vers = -1; then
    AC_COMPILE_IFELSE([#include "mrnet/MRNet.h"
      using namespace MRN;
      int main()
      {
        Network *net;
        net->register_EventCallback(Event::TOPOLOGY_EVENT, TopologyEvent::TOPOL_ADD_BE, NULL, NULL);
      }],
      [AC_DEFINE([MRNET3], [], [MRNet 3.X])
        AC_DEFINE([MRNET22], [], [MRNet 2.2]) 
        AC_DEFINE([MRNET2], [], [MRNet 2.X])
        mrnet_vers=3.X
      ]
    )
  fi
  if test $mrnet_vers = -1; then
    AC_COMPILE_IFELSE([#include "mrnet/MRNet.h"
      using namespace MRN;
      Network *net=Network::CreateNetworkFE(NULL, NULL, NULL);],
      [AC_DEFINE([MRNET22], [], [MRNet 2.2]) 
        AC_DEFINE([MRNET2], [], [MRNet 2.X])
        mrnet_vers=2.2
      ]
    )
  fi
  if test $mrnet_vers = -1; then
    AC_COMPILE_IFELSE([#include "mrnet/MRNet.h"
      using namespace MRN;
      NetworkTopology *topo;],
      [AC_DEFINE([MRNET2], [], [MRNet 2.X]) mrnet_vers=2.X]
    )
  fi
  AC_MSG_RESULT([$mrnet_vers])
  AC_MSG_CHECKING(for libmrnet)
  TMP_LDFLAGS=$LDFLAGS
  AC_ARG_WITH(cray-xt,
    [],
    [mrnet_libs="-lmrnet -lxplat -lpthread -ldl -lalps -lxmlrpc -lalpsutil"],
    [mrnet_libs="-lmrnet -lxplat -lpthread -ldl"]
  )  
  LDFLAGS="$LDFLAGS $mrnet_libs"
  AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "mrnet/MRNet.h"
    using namespace MRN;
    Network *net;)],
    [libmrnet_found=yes],
    [libmrnet_found=no]
  )
  LDFLAGS=$TMP_LDFLAGS
  AC_MSG_RESULT($libmrnet_found)
  if test "$libmrnet_found" = yes; then
    FELIBS="$FELIBS $mrnet_libs"
    BELIBS="$BELIBS $mrnet_libs"
  else
    AC_MSG_ERROR([libmrnet is required.  Specify libmrnet prefix with --with-mrnet])
  fi
  AC_LANG_POP(C++)
  AC_PATH_PROG([MRNETCOMMNODEBIN], [mrnet_commnode], [no], [$MRNETPREFIX/bin$PATH_SEPARATOR$PATH])
  if test $MRNETCOMMNODEBIN = no; then
    AC_MSG_ERROR([the mrnet_commnode executable is required.  Specify mrnet prefix with --with-mrnet])
  fi
])
AC_SUBST(MRNETCOMMNODEBIN)
