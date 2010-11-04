AC_DEFUN([X_AC_GRAPHLIB], [
  AC_ARG_WITH(graphlib, 
    [AS_HELP_STRING([--with-graphlib=prefix],
      [Add the compile and link search paths for graphlib]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
      LDFLAGS="$LDFLAGS -L${withval}/lib -Wl,-rpath=${withval}/lib"
    ],
    [CXXFLAGS="$CXXFLAGS"]
  )
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADER(graphlib.h,
    [],
    [AC_MSG_ERROR([graphlib.h is required.  Specify graphlib prefix with --with-graphlib])],
    AC_INCLUDES_DEFAULT
  )
  AC_CHECK_LIB(lnlgraph,graphlib_newGraph,liblnlgraph_found=yes,liblnlgraph_found=no)
  if test "$liblnlgraph_found" = yes; then
    CXXFLAGS="$CXXFLAGS -DSTAT_BITVECTOR -DGRL_DYNAMIC_NODE_NAME"
    FELIBS="$FELIBS -llnlgraph"
    BELIBS="$BELIBS -llnlgraph"
    MWLIBS="$MWLIBS -llnlgraph"
  else
    AC_MSG_ERROR([liblnlgraph is required.  Specify liblnlgraph prefix with --with-graphlib])
  fi
  AC_LANG_POP(C++)
])
