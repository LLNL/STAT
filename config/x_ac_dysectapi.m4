AC_DEFUN([X_AC_DYSECTAPI], [
  AM_CONDITIONAL([ENABLE_DYSECTAPI], false)
  AC_ARG_ENABLE(dysectapi,
    [AS_HELP_STRING([--enable-dysectapi],
      [Enable to include DysectAPI prototype]
    )],
    [CXXFLAGS="$CXXFLAGS -DDYSECTAPI -DGROUP_OPS"
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
])
