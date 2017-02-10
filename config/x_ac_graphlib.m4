AC_DEFUN([X_AC_GRAPHLIB], [

  AM_CONDITIONAL([ENABLE_GRAPHLIB20], false)
  AM_CONDITIONAL([ENABLE_GRAPHLIB30], false)

  AC_ARG_WITH(graphlib, 
    [AS_HELP_STRING([--with-graphlib=prefix],
      [Add the compile and link search paths for graphlib]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
      LDFLAGS="$LDFLAGS -L${withval}/lib"
      RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"
      GRAPHLIBPREFIX=${withval}
    ],
    [CXXFLAGS="$CXXFLAGS"]
  )
  AC_LANG_PUSH(C++)

  AC_CHECK_HEADER(graphlib.h,
    [],
    [AC_MSG_ERROR([graphlib.h is required.  Specify graphlib prefix with --with-graphlib])],
    AC_INCLUDES_DEFAULT
  )

  AC_MSG_CHECKING([Checking Graphlib Version])
  graphlib_vers=1
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([#include "graphlib.h"
    #include <stdlib.h>
    #include <stdio.h>
    int main()
    {
      graphlib_functiontable_p functions;
      return 0;
    }])],
    [AC_DEFINE([GRAPHLIB20], [], [Graphlib 2.0])
      graphlib_vers=2.0
      AM_CONDITIONAL([ENABLE_GRAPHLIB20], true)
    ]
  )
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([#include "graphlib.h"
    #include <stdlib.h>
    #include <stdio.h>
    int main()
    {
      int numNodeAttrs;
      graphlib_getNumNodeAttrs(NULL, &numNodeAttrs);
      return 0;
    }])],
    [AC_DEFINE([GRAPHLIB_3_0], [], [Graphlib 3.0])
      graphlib_vers=3.0
      AM_CONDITIONAL([ENABLE_GRAPHLIB30], true)
    ]
  )
  if test "$graphlib_vers" = 1 ; then
    AC_MSG_ERROR([STAT requires graphlib 2.0 or greater.  Specify graphlib prefix with --with-graphlib])
  fi
  AC_MSG_RESULT([$graphlib_vers])
  AC_CHECK_LIB(lnlgraph,graphlib_newGraph,liblnlgraph_found=yes,liblnlgraph_found=no)
  if test "$liblnlgraph_found" = yes; then
    CXXFLAGS="$CXXFLAGS"
    FELIBS="$FELIBS -llnlgraph"
    BELIBS="$BELIBS -llnlgraph"
    MWLIBS="$MWLIBS -llnlgraph"
  else
    AC_MSG_ERROR([liblnlgraph is required.  Specify liblnlgraph prefix with --with-graphlib])
  fi
  AC_LANG_POP(C++)
])

    graphlibError = graphlib_getNumNodeAttrs(retGraph, &numNodeAttrs);
