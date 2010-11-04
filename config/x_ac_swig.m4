AC_DEFUN([X_AC_SWIG], [ 
  AC_ARG_VAR(SWIG, [SWIG wrapper generator])
  AC_CHECK_PROGS(SWIG, swig)
  AC_SUBST(SWIG)
])

