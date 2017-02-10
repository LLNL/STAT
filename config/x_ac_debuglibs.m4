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
  AC_ARG_WITH(libelf,
    [AS_HELP_STRING([--with-libelf=prefix],
      [Add the compile and link search paths for libelf]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib"
     RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"]
  )
  AC_ARG_WITH(libiberty,
    [AS_HELP_STRING([--with-libiberty=prefix],
      [Add the compile and link search paths for libiberty]
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
  AC_ARG_WITH(ompd,
    [AS_HELP_STRING([--with-ompd=prefix],
      [Add the compile and link search paths for ompd]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
     LDFLAGS="$LDFLAGS -L${withval}/lib"
     RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"],
    [CXXFLAGS="$CXXFLAGS"]
  )
  AM_CONDITIONAL([ENABLE_OPENMP], false)
  AC_ARG_WITH(omp-stackwalker,
    [AS_HELP_STRING([--with-omp-stackwalker=prefix],
      [Add the compile and link search paths for openmp stack walker. Must have --with-stackwalker and --with-ompd defined]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include -DOMP_STACKWALKER"
     LDFLAGS="$LDFLAGS -L${withval}/lib"
     RPATH_FLAGS="$RPATH_FLAGS -Wl,-rpath=${withval}/lib"
     AM_CONDITIONAL([ENABLE_OPENMP], true)],
    [CXXFLAGS="$CXXFLAGS"]
  )

  AC_LANG_PUSH(C++)
  AC_CHECK_LIB(iberty,main,libiberty_found=yes,libiberty_found=no)
  if test "$libiberty_found" = yes; then
    BELIBS="$BELIBS -liberty"
  else
    AC_MSG_ERROR([libiberty is required.  Specify libiberty prefix with --with-libiberty])
  fi

  AC_CHECK_LIB(elf,elf_begin,libelf_found=yes,libelf_found=no)
  if test "$libelf_found" = yes; then
    BELIBS="$BELIBS -lelf"
  else
    AC_MSG_ERROR([libelf is required.  Specify libelf prefix with --with-libelf])
  fi

  AC_CHECK_LIB(dwarf,dwarf_init,libdwarf_found=yes,libdwarf_found=no,-lelf)
  if test "$libdwarf_found" = yes; then
    BELIBS="$BELIBS -ldwarf"
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

  AC_MSG_CHECKING(for libompd_intel)
  TMP_LDFLAGS=$LDFLAGS
  LDFLAGS="$LDFLAGS -lompd_intel"
  AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "ompd.h"
    ompd_callbacks_t *table;)],
    [libompd_found=yes],
    [libompd_found=no]
  )
  LDFLAGS=$TMP_LDFLAGS
  if test "$libompd_found" = yes; then
    BELIBS="-lompd_intel $BELIBS"
  fi
  AC_MSG_RESULT($libompd_found)

  AC_MSG_CHECKING(for libomp_stackwalker)
  TMP_LDFLAGS=$LDFLAGS
  LDFLAGS="$LDFLAGS -lomp_stackwalker $BELIBS"
  AC_LINK_IFELSE([AC_LANG_PROGRAM(#include "OpenMPStackWalker.h"
    OpenMPStackWalker* walker;)],
    [libomp_stackwalker_found=yes],
    [libomp_stackwalker_found=no]
  )
  LDFLAGS=$TMP_LDFLAGS
  if test "$libomp_stackwalker_found" = yes; then
    BELIBS="-lomp_stackwalker $BELIBS"
  fi
  AC_MSG_RESULT($libomp_stackwalker_found)

  AC_LANG_POP(C++)
])
