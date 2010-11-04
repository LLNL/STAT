AC_DEFUN([X_AC_ARCH], [
  AC_ARG_WITH(bluegene,
    [AS_HELP_STRING([--with-bluegene],
        [Add the flags to run on a BlueGene system]
    )],
    [AC_DEFINE([BGL], [], [Compilation for BlueGene systems])]
    [CXXFLAGS="$CXXFLAGS"]
  )  
  AC_ARG_WITH(cray-xt,
    [AS_HELP_STRING([--with-cray-xt],
      [Add the flags to run on a Cray XT system]
    )],
    [AC_DEFINE([CRAYXT], [], [Compilation for CrayXT systems])]
    [CXXFLAGS="$CXXFLAGS"]
  )  
])
