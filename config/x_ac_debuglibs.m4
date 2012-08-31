AC_DEFUN([X_AC_DEBUGLIBS], [
  AC_ARG_WITH(stackwalker,
    [AS_HELP_STRING([--with-stackwalker=prefix],
      [Add the compile and link search paths for stackwalker]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib"
     RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"]
  )
  AC_ARG_WITH(libdwarf, 
    [AS_HELP_STRING([--with-libdwarf=prefix],  
      [Add the compile and link search paths for libdwarf]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib"
     RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"]
  )

  AC_LANG_PUSH(C++)
  AC_CHECK_LIB(dwarf,dwarf_init,libdwarf_found=yes,libdwarf_found=no,-lelf)
  if test "$libdwarf_found" = yes; then
    BELIBS="$BELIBS -ldwarf -lelf -liberty"
  else
    AC_MSG_ERROR([libdwarf is required.  Specify libdwarf prefix with --with-libdwarf])
  fi

  AC_CHECK_HEADER(walker.h,
    [],
    [AC_MSG_ERROR([walker.h is required.  Specify stackwalker prefix with --with-stackwalker])],
    AC_INCLUDES_DEFAULT
  )
  AC_MSG_CHECKING(for libstackwalk)
  TMP_LDFLAGS=$LDFLAGS
  LDFLAGS="$LDFLAGS -lstackwalk -lpcontrol -lparseAPI -linstructionAPI -lsymtabAPI -lcommon -ldwarf -lelf -liberty -lpthread"
  AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "walker.h"
    using namespace Dyninst;
    using namespace Dyninst::Stackwalker;
    Walker *walker;)],
    [libstackwalk_found=yes],
    [libstackwalk_found=no]
  )
  LDFLAGS=$TMP_LDFLAGS
  if test "$libstackwalk_found" = yes; then
    BELIBS="-lstackwalk -lpcontrol -lparseAPI -linstructionAPI -lsymtabAPI -lcommon -liberty $BELIBS"
  else
    LDFLAGS="$LDFLAGS -lstackwalk -lsymtabAPI -lcommon -ldwarf -lelf -liberty -lpthread"
    AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "walker.h"
      using namespace Dyninst;
      using namespace Dyninst::Stackwalker;
      Walker *walker;)],
      [libstackwalk_found=yes],
      [libstackwalk_found=no]
    )
    LDFLAGS=$TMP_LDFLAGS
    if test "$libstackwalk_found" = yes; then
      BELIBS="-lstackwalk -lsymtabAPI -lcommon -liberty $BELIBS"
    else
      AC_MSG_ERROR([libstackwalk is required.  Specify libstackwalk prefix with --with-stackwalker])
    fi
  fi
  AC_MSG_RESULT($libstackwalk_found)
  AC_LANG_POP(C++)
])
