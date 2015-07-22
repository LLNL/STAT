AC_DEFUN([X_AC_DYSECTAPI], [
  AM_CONDITIONAL([ENABLE_DYSECTAPI], false)
  AC_ARG_ENABLE(dysectapi,
    [AS_HELP_STRING([--enable-dysectapi],
      [Enable to include DysectAPI prototype]
    )],
    [ENABLE_DYSECT="yes"
      CXXFLAGS="$CXXFLAGS -DDYSECTAPI"
      AM_CONDITIONAL([ENABLE_DYSECTAPI], true)
      AC_SUBST(LIBDYSECTFE, [-lDysectFE])
      AC_SUBST(LIBDYSECTBE, [-lDysectBE])
      AC_SUBST(LIBDYSECTLOC, [dysect/libDysectAPI/src])
      AC_SUBST(LIBDYSECTAPIFE, [-lDysectAPIFE])
      AC_SUBST(LIBDYSECTAPIBE, [-lDysectAPIBE])
      AC_SUBST(LIBDYSECTAPIFD, [-lDysectAPIFD])
      AC_SUBST(LIBDYSECTAPILOC, [dysect/DysectAPI])
    ],
    [CXXFLAGS="$CXXFLAGS"]
  )

  AM_CONDITIONAL([ENABLE_DEPCORE], false)
  AC_ARG_WITH(depcore,
    [AS_HELP_STRING([--with-depcore=prefix],
      [Add the compile and link search paths for libdepositcore]
    )],
    [ CXXFLAGS="$CXXFLAGS -DDYSECTAPI_DEPCORE"
      DEPCOREPREFIX="${withval}"
      WITH_DEPCORE=yes
      AM_CONDITIONAL([ENABLE_DEPCORE], true)
    ],
    [CXXFLAGS="$CXXFLAGS"
      WITH_DEPCORE=no
      DEPCOREPREFIX=""
    ]
  )
  if test "$WITH_DEPCORE" = yes
  then
    AC_LANG_PUSH(C)
    AC_MSG_CHECKING(for libdepositcore)
    TMP_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS -L${withval}/lib -ldepositcore"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern void LDC_Depcore_cont(int rank, int safemode);]],
      [[LDC_Depcore_cont(0, 0);]])],
      [libdepcore_found=yes],
      [libdepcore_found=no]
    )
    LDFLAGS=$TMP_LDFLAGS
    AC_MSG_RESULT($libdepcore_found)
    if test "$libdepcore_found" = no; then
      AC_MSG_ERROR([libdepositcore is required.  Specify libdepositcore prefix with --with-depcore])
    fi
  fi
])
