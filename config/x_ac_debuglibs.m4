AC_DEFUN([X_AC_DEBUGLIBS], [

  AC_ARG_ENABLE(stackwalker-rpm,
    [AS_HELP_STRING([--enable-stackwalker-rpm],[Enable the use of rpm-installed stackwalker, default=no])],
    [CXXFLAGS="$CXXFLAGS -I/usr/include/dyninst"
     LDFLAGS="$LDFLAGS -L/usr/lib64/dyninst"
     STACKWALKERPREFIX="${withval}"
     RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=/usr/lib64/dyninst"],
    [CXXFLAGS="$CXXFLAGS"
     STACKWALKERPREFIX="${withval}"]
  )  
  AC_ARG_ENABLE(libdwarf-rpm,
    [AS_HELP_STRING([--enable-libdwarf-rpm],[Enable the use of rpm-installed libdwarf, default=no])],
    [CXXFLAGS="$CXXFLAGS -I/usr/include/libdwarf"],
    [CXXFLAGS="$CXXFLAGS"]
  )  

  AC_ARG_WITH(stackwalker,
    [AS_HELP_STRING([--with-stackwalker=prefix],
      [Add the compile and link search paths for stackwalker]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib"
     STACKWALKERPREFIX="${withval}"
     RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"
     STACKWALKERPREFIX="${withval}"]
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
  LDFLAGS="$LDFLAGS -ldyninstAPI -lstackwalk -lpcontrol -lparseAPI -linstructionAPI -lsymtabAPI -lcommon -ldynElf -ldynDwarf -lsymLite -ldwarf -lelf -liberty -lpthread"
  AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "walker.h"
    using namespace Dyninst;
    using namespace Dyninst::Stackwalker;
    Walker *walker;)],
    [libstackwalk_found=yes],
    [libstackwalk_found=no]
  )
  LDFLAGS=$TMP_LDFLAGS
  if test "$libstackwalk_found" = yes; then
    BELIBS="-ldyninstAPI -lstackwalk -lpcontrol -lparseAPI -linstructionAPI -lsymtabAPI -lcommon -ldynElf -ldynDwarf -lsymLite -liberty $BELIBS"
  else
    LDFLAGS="$LDFLAGS -lstackwalk -lsymtabAPI -lpcontrol -lparseAPI -linstruction -lcommon -ldwarf -lelf -liberty -lpthread"
    AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "walker.h"
      using namespace Dyninst;
      using namespace Dyninst::Stackwalker;
      Walker *walker;)],
      [libstackwalk_found=yes],
      [libstackwalk_found=no]
    )
    LDFLAGS=$TMP_LDFLAGS
    if test "$libstackwalk_found" = yes; then
      BELIBS="-ldyninstAPI -lstackwalk -lsymtabAPI -lpcontrol -lparseAPI -linstruction -lcommon -liberty $BELIBS"
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
        BELIBS="-ldyninstAPI -lstackwalk -lsymtabAPI -lcommon -liberty $BELIBS"
      else
        AC_MSG_ERROR([libstackwalk is required.  Specify libstackwalk prefix with --with-stackwalker])
      fi
    fi
  fi
  AC_SUBST(STACKWALKERPREFIX)
  AC_MSG_RESULT($libstackwalk_found)
  AC_LANG_POP(C++)
])
