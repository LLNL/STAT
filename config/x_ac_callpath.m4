AC_DEFUN([X_AC_CALLPATH], [
  AM_CONDITIONAL([ENABLE_CALLPATH], false)
  AC_ARG_WITH(adeptutils, 
    [AS_HELP_STRING([--with-adeptutils=prefix],
      [Add the compile and link search paths for adeptutils]
    )],
    [ADEPTUTILSPREFIX="${withval}"],
    [ADEPTUTILSPREFIX=""]
  )
  AC_SUBST(ADEPTUTILSPREFIX)
  AC_ARG_WITH(callpath, 
    [AS_HELP_STRING([--with-callpath=prefix],
      [Add the compile and link search paths for callpath]
    )],
    [CALLPATHPREFIX="${withval}"
      WITH_CALLPATH=yes
      CXXFLAGS="$CXXFLAGS -DCALLPATH_ENABLED -fpermissive"
      AM_CONDITIONAL([ENABLE_CALLPATH], true)
    ],
    [WITH_CALLPATH=no
      CALLPATHPREFIX=""
    ]
  )
  if test "$WITH_CALLPATH" = yes
  then
    AC_LANG_PUSH(C++)
    
    AC_ARG_WITH(mpih,
      [AS_HELP_STRING([--with-mpih=path],
        [Specify the path to the mpi.h header file]
      )],
      [MPIH_HEADERPATH="${withval}"],
      [AC_MSG_ERROR([MPI header file location required when using callpath, please specify with --with-mpih])]
    )
    AC_SUBST(MPIH_HEADERPATH)

    X_AC_MPICC
    TMP_CXX=$CXX
    CXX=$MPICXX
    TMP_CXXFLAGS=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS -I${CALLPATHPREFIX}/include -I${ADEPTUTILSPREFIX}/include -I${MPIH_HEADERPATH}"
    AC_CHECK_HEADER(CallpathRuntime.h,
      [],
      [AC_MSG_ERROR([CallpathRuntime.h is required.  Specify callpath prefix with --with-callpath])],
      AC_INCLUDES_DEFAULT
    )
  
    AC_MSG_CHECKING(for libcallpath)
    TMP_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS -L${CALLPATHPREFIX}/lib -lcallpath"
    AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "CallpathRuntime.h"
      CallpathRuntime runtime;)],
      [libcallpath=yes],
      [libcallpath=no]
    )
    LDFLAGS=$TMP_LDFLAGS
    CXXFLAGS=$TMP_CXXFLAGS
    CXX=$TMP_CXX
    AC_MSG_RESULT($libcallpath)
    if test "$libcallpath" != "yes"; then
      AC_MSG_ERROR([libcallpath is required.  Specify libcallpath prefix with --with-callpath])
    fi

    AC_LANG_POP(C++)
  fi
  AC_SUBST(CALLPATHPREFIX)
])

