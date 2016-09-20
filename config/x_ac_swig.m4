AC_DEFUN([X_AC_SWIG], [ 
  AC_ARG_VAR(SWIG, [SWIG wrapper generator])
  AC_CHECK_PROGS(SWIG, swig, "no")
  if test "x$SWIG" = "xno"; then
    AC_MSG_ERROR([swig is required to build the STAT GUI.])
  fi
  AC_SUBST(SWIG)
])

