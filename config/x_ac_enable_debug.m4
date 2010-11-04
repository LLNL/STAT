AC_DEFUN([X_AC_ENABLE_DEBUG], [  
  AC_MSG_CHECKING([whether to enable debug build])
  AC_ARG_ENABLE([debug], 
    AS_HELP_STRING(--enable-debug,enable debug build), 
    [if test "x$enableval" = "xyes"; then
      AC_DEFINE(DEBUG,1,[Define DEBUG flag])
      case "$CXXFLAGS" in
        *-O2*)
          CXXFLAGS=`echo $CXXFLAGS| sed s/-O2//g`
          ;;
        *-O*)
          CXXFLAGS=`echo $CXXFLAGS| sed s/-O//g`
          ;;
        *)
          ;;
      esac
      CFLAGS="-g $CFLAGS"
      CXXFLAGS="-g $CXXFLAGS"
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi],
    [AC_MSG_RESULT([no])]
  )
])
