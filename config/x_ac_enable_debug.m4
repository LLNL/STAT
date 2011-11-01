AC_DEFUN([X_AC_ENABLE_DEBUG], [  
  AC_MSG_CHECKING([whether to enable debug build])
  AC_ARG_ENABLE([debug], 
    AS_HELP_STRING(--enable-debug,enable debug build), 
    [if test "x$enableval" = "xyes"; then
      AC_DEFINE(DEBUG,1,[Define DEBUG flag])
      case "$CFLAGS" in
        *-O2*)
          CFLAGS=`echo $CFLAGS| sed s/-O2//g`
          ;;
        *-O*)
          CFLAGS=`echo $CFLAGS| sed s/-O//g`
          ;;
        *)
          ;;
      esac
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
      CFLAGS="-g -O0 $CFLAGS"
      CXXFLAGS="-g -O0 $CXXFLAGS"
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi],
    [AC_MSG_RESULT([no])]
  )
])
