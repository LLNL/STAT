AC_DEFUN([X_AC_DYSECTAPI], [
  AM_CONDITIONAL([ENABLE_DYSECTAPI], false)
  AC_ARG_ENABLE(dysectapi, 
    [AS_HELP_STRING([--enable-dysectapi],
      [Enable to include DysectAPI prototype]
    )],
    [CXXFLAGS="$CXXFLAGS -DDYSECTAPI -DGROUP_OPS"
      AM_CONDITIONAL([ENABLE_DYSECTAPI], true)
    ],
    [CXXFLAGS="$CXXFLAGS"]
  )
])
