AC_DEFUN([X_AC_TEMPORALORDERINGAPI], [
  AC_ARG_WITH(to_api,
    [AS_HELP_STRING([--with-to_api=prefix],
      [Add the compile and link search paths for temporal ordering api]
    )],
    [CXXFLAGS="$CXXFLAGS -DPROTOTYPE"
      WITH_TEMPORALORDERINGAPI=yes
      to_prefix=${withval}
    ],
    [CXXFLAGS="$CXXFLAGS"
      WITH_TEMPORALORDERINGAPI=no
    ]
  )
  if test "$WITH_TEMPORALORDERINGAPI" = yes
  then
    X_AC_BOOST
    if test "$boost_found" = no
    then
      AC_ERROR([Boost is required for temporal ordering.  Specify Boost prefix with --with-boost])
    fi

    X_AC_ROSE
    if test "$rose_found" = no
    then
      AC_ERROR([Rose is required for temporal ordering.  Specify Rose prefix with --with-rose])
    fi

    AC_MSG_CHECKING([Temporal Ordering API installation])
    if test -f "$to_prefix"/include/TO_std.h ; then
      AC_MSG_RESULT([yes])
      AC_SUBST(TOAPI_INCLUDE, -I$to_prefix/include)
    else
      AC_MSG_RESULT([no])
      AC_ERROR([TO_std.h is required for temporal ordering.  Specify Temporal Ordering API prefix with --with-to_api])
    fi

    AC_MSG_CHECKING(for libTemporalOrderAPI)
    if test -f "$to_prefix"/lib/libTemporalOrderAPI.so ; then
      AC_MSG_RESULT([yes])
      AC_SUBST(LIBTOAPIDIR, [$to_prefix/lib])
      AC_SUBST(LIBTOAPI,["-lTemporalOrderAPI"])
    else
      AC_MSG_RESULT([no])
      AC_ERROR([TO_std.h is required for temporal ordering.  Specify Temporal Ordering API prefix with --with-to_api])
    fi
  fi
  AM_CONDITIONAL([ENABLE_TO], [test "$WITH_TEMPORALORDERINGAPI" = yes])
])
