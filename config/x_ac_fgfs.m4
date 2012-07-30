AC_DEFUN([X_AC_FGFS], [
  AC_ARG_WITH(fgfs, 
    [AS_HELP_STRING([--with-fgfs=prefix],
      [Add the compile and link search paths for fgfs]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include -DSTAT_FGFS"
      FGFSPREFIX="${withval}"
      WITH_FGFS=yes
      LDFLAGS="$LDFLAGS -L${withval}/lib"
      RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"
    ],
    [CXXFLAGS="$CXXFLAGS"
      WITH_FGFS=no
      FGFSPREFIX=""
    ]
  )
  if test "$WITH_FGFS" = yes
  then
    AC_LANG_PUSH(C++)
    AC_CHECK_HEADER(FastGlobalFileStat.h,
      [],
      [AC_MSG_ERROR([FastGlobalFileStat.h is required.  Specify fgfs prefix with --with-fgfs])],
      AC_INCLUDES_DEFAULT
    )
  
    AC_MSG_CHECKING(for libfgfs_mrnet)
    TMP_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS -lfgfs_mrnet"
    AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "AsyncFastGlobalFileStat.h"
      using namespace FastGlobalFileStat;
      GlobalFileStatBase *ptr;)],
      [libfgfs_mrnet_found=yes],
      [libfgfs_mrnet_found=no]
    )
    LDFLAGS=$TMP_LDFLAGS
    AC_MSG_RESULT($libfgfs_mrnet_found)
    if test "$libfgfs_mrnet_found" = yes; then
      FELIBS="$FELIBS -lfgfs_mrnet -lelf"
      BELIBS="$BELIBS -lfgfs_mrnet"
    else
      AC_MSG_ERROR([libfgfs_mrnet is required.  Specify libfgfs_mrnet prefix with --with-fgfs])
    fi
    
    AC_LANG_POP(C++)
    AC_PATH_PROG([FGFSFILTERPATH], [libfgfs_filter.so], [no], [$FGFSPREFIX/lib$PATH_SEPARATOR$PATH])
    if test $FGFSFILTERPATH = no; then
      AC_MSG_ERROR([the libfgfs_filter.so shared object is required and was not found in $FGFSPREFIX/lib$PATH_SEPARATOR$PATH.  Specify fgfs prefix with --with-fgfs])
    fi
  fi
AC_SUBST(FGFSFILTERPATH)
])
