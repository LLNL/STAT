AC_DEFUN([X_AC_GRAPHVIZ], [
  AC_ARG_WITH(graphviz,
    [AS_HELP_STRING([--with-graphviz=prefix],
      [Add the search path, prefix to bin/dot, for graphviz]
    )],
    [DOTBINDIR="${withval}/bin"],
    [DOTBINDIR="no"]
  )
  if test $DOTBINDIR = "no"; then
    AC_PATH_PROG([DOTFOUND], [dot], [no])
    if test $DOTFOUND = no; then
      AC_MSG_ERROR([the dot executable from graphviz is required.  Specify graphviz prefix with --with-graphviz])
    fi
    DOTBINDIR=`dirname $DOTFOUND`
  else
    AC_CHECK_FILE([${DOTBINDIR}/dot],
      [AC_MSG_RESULT([yes])],
      [AC_MSG_ERROR([the dot executable from graphviz not found in ${DOTBINDIR}/dot.  Specify graphviz prefix with --with-graphviz])]
    )
  fi
])
AC_SUBST(DOTBINDIR)
