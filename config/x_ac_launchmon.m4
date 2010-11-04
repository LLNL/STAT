AC_DEFUN([X_AC_LAUNCHMON], [
  AC_ARG_WITH(launchmon,
    [AS_HELP_STRING([--with-launchmon=prefix],
      [Add the compile and link search paths for launchmon]
    )],
    [CXXFLAGS="$CXXFLAGS -I${withval}/include"
       LAUNCHMONPREFIX="${withval}"
       LDFLAGS="$LDFLAGS -L${withval}/lib -Wl,-rpath=${withval}/lib"
    ],
    [CXXFLAGS="$CXXFLAGS"
      LAUNCHMONPREFIX="/usr/local"
    ]
  )
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADERS(lmon_api/lmon_fe.h, 
    [],
    [AC_MSG_ERROR([lmon_api/lmon_fe.h is required.  Specify launchmon prefix with --with-launchmon])],
    AC_INCLUDES_DEFAULT
  )
  AC_CHECK_HEADERS(lmon_api/lmon_be.h, 
    [], 
    [AC_MSG_ERROR([lmon_api/lmon_be.h is required.  Specify launchmon prefix with --with-launchmon])],
    AC_INCLUDES_DEFAULT
  )
  AC_CHECK_LIB(monfeapi,LMON_fe_createSession,libmonfeapi_found=yes,libmonfeapi_found=no)
  if test "$libmonfeapi_found" = yes; then
    FELIBS="$FELIBS -lmonfeapi"
  else
    AC_MSG_ERROR([libmonfeapi is required.  Specify libmonfeapi prefix with --with-launchmon])
  fi
  AC_CHECK_LIB(monbeapi,LMON_be_init,libmonbeapi_found=yes,libmonbeapi_found=no)
  if test "$libmonbeapi_found" = yes; then
    BELIBS="$BELIBS -lmonbeapi"
  else
    AC_MSG_ERROR([libmonbeapi is required.  Specify libmonbeapi prefix with --with-launchmon])
  fi
  AC_LANG_POP(C++)
  AC_PATH_PROG([LAUNCHMONBIN], [launchmon], [no], [$LAUNCHMONPREFIX/bin])
  if test $LAUNCHMONBIN = no; then
    AC_MSG_ERROR([the launchmon executable is required.  Specify launchmon prefix with --with-launchmon])
  fi
AC_SUBST(LAUNCHMONBIN)
])
