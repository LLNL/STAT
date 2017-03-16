#!/bin/bash
#
#

set -ex

prefix=$HOME/local
cachedir=$HOME/local/.cache

#
# Ordered list of software to download and install into $prefix.
#  NOTE: Code currently assumes .tar.gz suffix...
#
downloads="\
https://www.open-mpi.org/software/ompi/v2.0/downloads/openmpi-2.0.2.tar.gz \
https://github.com/LLNL/LaunchMON/releases/download/v1.0.2/launchmon-v1.0.2.tar.gz \
https://github.com/LLNL/graphlib/archive/v3.0.0.tar.gz \
https://github.com/dyninst/mrnet/archive/MRNet-4_1_0.tar.gz \
https://www.prevanders.net/libdwarf-20161124.tar.gz \
https://github.com/dyninst/dyninst/archive/v9.3.0.tar.gz"

declare -A extra_configure_opts=(\
["launchmon-v1.0.2"]="--with-test-rm=orte --with-test-ncore-per-CN=2" \
["MRNet-4_1_0"]="--enable-shared" \
["libdwarf-20161124"]="--enable-shared --disable-nonshared" \
)

declare -A extra_cmake_opts=(\
["v9.3.0"]="-D CMAKE_INSTALL_PREFIX=$HOME/local -D LIBDWARF_INCLUDE_DIR=$HOME/local/include -D LIBDWARF_LIBRARIES=$HOME/local/lib/libdwarf.so"
)

declare -r prog=${0##*/}
declare -r long_opts="prefix:,cachedir:,verbose,printenv"
declare -r short_opts="vp:c:P"
declare -r usage="\
\n
Usage: $prog [OPTIONS]\n\
Download and install to a local prefix (default=$prefix) dependencies\n\
for building STAT\n\
\n\
Options:\n\
 -v, --verbose           Be verbose.\n\
 -P, --printenv          Print environment variables to stdout\n\
 -c, --cachedir=DIR      Check for precompiled dependency cache in DIR\n\
 -e, --max-cache-age=N   Expire cache in N days from creation\n\
 -p, --prefix=DIR        Install software into prefix\n
"

die() { echo -e "$prog: $@"; exit 1; }
say() { echo -e "$prog: $@"; }
debug() { test "$verbose" = "t" && say "$@"; }

GETOPTS=`/usr/bin/getopt -u -o $short_opts -l $long_opts -n $prog -- $@`
if test $? != 0; then
    die "$usage"
fi

eval set -- "$GETOPTS"
while true; do
    case "$1" in
      -v|--verbose)          verbose=t;     shift   ;;
      -c|--cachedir)         cachedir="$2"; shift 2 ;;
      -e|--max-cache-age)    cacheage="$2"; shift 2 ;;
      -p|--prefix)           prefix="$2";   shift 2 ;;
      -P|--printenv)         print_env=1;   shift   ;;
      --)                    shift ; break;         ;;
      *)                     die "Invalid option '$1'\n$usage" ;;
    esac
done

print_env () {
    echo "export LD_LIBRARY_PATH=${prefix}/lib:$LD_LIBRARY_PATH"
    echo "export CPPFLAGS=-I${prefix}/include"
    echo "export LDFLAGS=-L${prefix}/lib"
    echo "export PATH=${PATH}:${HOME}/local/usr/bin:${HOME}/local/bin"
    luarocks path --bin
}

if test -n "$print_env"; then
    print_env
    exit 0
fi

eval $(print_env)

sanitize ()
{
    # Sanitize cache name
    echo $1 | sed 's/[\t /\]/_/g'
}

check_cache ()
{
    test -n "$cachedir" || return 1
    local url=$(sanitize $1)
    local cachefile="${cachedir}/${url}"
    test -f "${cachefile}" || return 1
    test -n "$cacheage"    || return 0

    local ctime=$(stat -c%Y ${cachefile})
    local maxage=$((${cacheage}*86400))
    test $ctime -gt $maxage && return 1
}

add_cache ()
{
    test -n "$cachedir" || return 0
    mkdir -p "${cachedir}" &&
    touch "${cachedir}/$(sanitize ${1})"
}

for pkg in $downloads; do
    name=$(basename ${pkg} .tar.gz)
    cmake_opts="${extra_cmake_opts[$name]}"
    configure_opts="${extra_configure_opts[$name]}"
    cache_name="$name:$sha1:$make_opts:$configure_opts:$cmake_opts"
    # note that we need to build openmpi and STAT's examples with gfortran installed
    # however, having this package causes dyninst to fail to build
    #if test "$name" = "openmpi-2.0.2"; then
    #if test "$name" = "v9.3.0"; then
    #   say "rebuiding ${name}"
    #else
    if check_cache "$name"; then
       say "Using cached version of ${name}"
       continue
    fi
    #fi
    mkdir -p ${name}  || die "Failed to mkdir ${name}"
    (
      cd ${name} &&
      curl -L -O --insecure ${pkg} || die "Failed to download ${pkg}"
      tar --strip-components=1 -xf *.tar.gz || die "Failed to un-tar ${name}"
      if test -x configure; then
        cc=gcc cxx=g++ ./configure --prefix=${prefix} \
                       --sysconfdir=${prefix}/etc \
                       $configure_opts
      elif test -f CMakeLists.txt; then
        mkdir build && cd build
        cmake -DCMAKE_INSTALL_PREFIX=${prefix} $cmake_opts ..
      fi
      make PREFIX=${prefix} &&
      make PREFIX=${prefix} install
      if test "$name" = "libdwarf-20161124"; then
        pwd
        ls
        pushd libdwarf
        cp libdwarf.so $HOME/local/lib
        cp libdwarf.so.1 $HOME/local/lib
        cp libdwarf.h  $HOME/local/include
        cp dwarf.h $HOME/local/include
        popd
      fi
      if test "$name" = "v9.3.0"; then
        cp libiberty/libiberty.a $HOME/local/lib
      fi
    ) || die "Failed to build and install $name"
    add_cache "$name"
done


