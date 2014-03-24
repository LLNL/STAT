AC_DEFUN([X_AC_ROSE], [

  AC_MSG_CHECKING([ROSE installation])

  AC_ARG_WITH([rose],
    AS_HELP_STRING(--with-rose@<:@=rose installation path@:>@,specify the path to rose @<:@default=/usr@:>@), 
    [with_rose_path=$withval],
    [with_rose_path="check"]
  )

  rose_dflt_dir="/usr"
  rose_found="no" 
  if test "x$with_rose_path" != "xcheck"; then
    #rose install root given
    #
    if test -f "$with_rose_path"/include/rose.h ; then
      AC_SUBST(ROSE_INCLUDE, -I$with_rose_path/include)
      AC_SUBST(LIBROSEDIR, [$with_rose_path/lib])
      AC_SUBST(LIBJAVADIR, [/etc/alternatives/java_sdk_gcj/jre/lib/amd64/server])
      AC_SUBST(LIBROSE, [-lrose])
      AC_SUBST(LIBJVM, [-ljvm])
      AC_DEFINE(HAVE_ROSE_H,1,[Define 1 if rose.h is found])
      rose_found="yes"
    else
      rose_found="no"
    fi
  else 
    # rose path not given
    if test -f $rose_dflt_dir/include/rose.hpp ; then
      AC_SUBST(ROSE_INCLUDE, -I$rose_dflt_dir/include)
      AC_SUBST(LIBROSEDIR, [$rose_dflt_dir/lib])
      AC_SUBST(LIBJAVADIR, [/etc/alternatives/java_sdk_gcj/jre/lib/amd64/server])
      AC_SUBST(LIBROSE, [-lrose])
      AC_SUBST(LIBJVM, [-ljvm])
      AC_DEFINE(HAVE_ROSE_H,1,[Define 1 if rose.h is found])
      AC_DEFINE(HAVE_ROSE_H,1,[Define 1 if rose.h is found])
      rose_found="yes"
    else
      rose_found="no"
    fi
  fi
  AC_MSG_RESULT($rose_found)
])
