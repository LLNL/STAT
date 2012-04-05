AC_DEFUN([X_AC_DEBUGLIBS], [
  AC_ARG_ENABLE(dyninst, 
    [AS_HELP_STRING([--enable-dyninst], 
      [Use Dyninst for stack sampling, default is StackWalker]
    )],
    [enable_dyninst=yes],
    [AC_DEFINE([STACKWALKER], [], [Use StackWalker]) enable_dyninst=no]
  )
  AC_ARG_WITH(stackwalker,
    [AS_HELP_STRING([--with-stackwalker=prefix],
      [Add the compile and link search paths for stackwalker]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"]
  )
  AC_ARG_WITH(dyninst,
    [AS_HELP_STRING([--with-dyninst=prefix],
      [Add the compile and link search paths for dyninst]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"]
  )
  AC_ARG_WITH(libdwarf, 
    [AS_HELP_STRING([--with-libdwarf=prefix],  
      [Add the compile and link search paths for libdwarf]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"]
  )

  AC_LANG_PUSH(C++)
  AC_CHECK_LIB(dwarf,dwarf_init,libdwarf_found=yes,libdwarf_found=no,-lelf)
  if test "$libdwarf_found" = yes; then
    BELIBS="$BELIBS -ldwarf -lelf -liberty"
  else
    AC_MSG_ERROR([libdwarf is required.  Specify libdwarf prefix with --with-libdwarf])
  fi
  if test "$enable_dyninst" = yes; then
    AC_CHECK_HEADER(BPatch.h
      [],
      [AC_MSG_ERROR([BPatch.h is required.  Specify dyninst prefix with --with-dyninst])],
      AC_INCLUDES_DEFAULT
    )
    AC_MSG_CHECKING(for libdyninstAPI)
    TMP_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS -ldyninstAPI -lsymtabAPI -lcommon -ldwarf -lelf -liberty"
    AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "BPatch_process.h"
      BPatch_process *proc;)],
      [libdyninstapi_found=yes],
      [libdyninstapi_found=no]
    )
    LDFLAGS=$TMP_LDFLAGS
    AC_MSG_RESULT($libdyninstAPI_found)
    if test "$libdyninstapi_found" = yes; then
      BELIBS="-ldyninstAPI -lsymtabAPI -lcommon -liberty $BELIBS"
    else
      AC_MSG_ERROR([libdyninstAPI is required.  Specify libdyninstAPI prefix with --with-dyninst])
    fi
  else
    AC_CHECK_HEADER(walker.h,
      [],
      [AC_MSG_ERROR([walker.h is required.  Specify stackwalker prefix with --with-stackwalker])],
      AC_INCLUDES_DEFAULT
    )
    AC_MSG_CHECKING(for libstackwalk)
    TMP_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS -lstackwalk -lpcontrol -lparseAPI -lsymtabAPI -lcommon -ldwarf -lelf -liberty -lpthread"
    AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "walker.h"
      using namespace Dyninst;
      using namespace Dyninst::Stackwalker;
      Walker *walker;)],
      [libstackwalk_found=yes],
      [libstackwalk_found=no]
    )
    LDFLAGS=$TMP_LDFLAGS
    AC_MSG_RESULT($libstackwalk_found)
    if test "$libstackwalk_found" = yes; then
      BELIBS="-lstackwalk -lpcontrol -lparseAPI -lsymtabAPI -lcommon -liberty $BELIBS"
    else
      AC_MSG_ERROR([libstackwalk is required.  Specify libstackwalk prefix with --with-stackwalker])
    fi
  fi
  AC_LANG_POP(C++)
])
