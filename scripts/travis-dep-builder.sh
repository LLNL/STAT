#!/bin/bash
#
#

set -ex

prefix=$HOME/local
cachedir=$HOME/local/.cache
components=all

#
# Ordered list of software to download and install into $prefix.
#  NOTE: Code currently assumes .tar.gz suffix...
#
#https://github.com/LLNL/LaunchMON/releases/download/v1.0.2/launchmon-v1.0.2.tar.gz \
# NOTE: openmpi and dyninst both take a long time to build and will likely cause travis to timeout after 50 minutes, so they should be built in two passes
downloads="\
https://www.open-mpi.org/software/ompi/v2.0/downloads/openmpi-2.0.3.tar.gz \
https://github.com/LLNL/graphlib/archive/v3.0.0.tar.gz \
ftp://ftp.cs.wisc.edu/paradyn/mrnet/mrnet_5.0.1.tar.gz \
https://www.prevanders.net/libdwarf-20161124.tar.gz \
https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz \
https://github.com/dyninst/dyninst/archive/v9.3.2.tar.gz"

checkouts="\
https://github.com/llnl/launchmon.git"

declare -A checkout_sha1=(\
["launchmon"]="a7799a90e8e0a7eaa335135569e0762cc0f0c99d"
)

declare -A extra_configure_opts=(\
["launchmon-v1.0.2"]="--with-test-rm=orte --with-test-ncore-per-CN=2 --with-test-nnodes=1 --with-test-rm-launcher=${prefix}/bin/mpirun --with-test-installed" \
["launchmon"]="--with-test-rm=orte --with-test-ncore-per-CN=2 --with-test-nnodes=1 --with-test-rm-launcher=${prefix}/bin/mpirun --with-test-installed" \
["mrnet_5.0.1"]="--enable-shared" \
["libdwarf-20161124"]="--enable-shared --disable-nonshared" \
)

# we install dyninst in a separate prefix b/c otherwise it will break on previously installed headers
declare -A extra_cmake_opts=(\
["v9.3.2"]="-D CMAKE_INSTALL_PREFIX=${prefix}/dyninst -D LIBDWARF_INCLUDE_DIR=${prefix}/include -D LIBDWARF_LIBRARIES=${prefix}/lib/libdwarf.so"
)

declare -r prog=${0##*/}
declare -r long_opts="prefix:,cachedir:,verbose,printenv,components:"
declare -r short_opts="vp:c:PC:"
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
 -C, --components=VAL    VAL=[ompi|dyninst|other]\n
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
      -v|--verbose)          verbose=t;       shift   ;;
      -c|--cachedir)         cachedir="$2";   shift 2 ;;
      -e|--max-cache-age)    cacheage="$2";   shift 2 ;;
      -p|--prefix)           prefix="$2";     shift 2 ;;
      -P|--printenv)         print_env=1;     shift   ;;
      -C|--components)       components="$2"; shift 2 ;;
      --)                    shift ; break;         ;;
      *)                     die "Invalid option '$1'\n$usage" ;;
    esac
done

print_env () {
    echo "export LD_LIBRARY_PATH=${prefix}/lib:$LD_LIBRARY_PATH"
    echo "export CPPFLAGS=-I${prefix}/include"
    echo "export LDFLAGS=-L${prefix}/lib"
    echo "export PATH=${PATH}:${prefix}/usr/bin:${prefix}/bin"
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

env

for pkg in $downloads; do
    name=$(basename ${pkg} .tar.gz)
    if test "$components" = "dyninst" -a "$name" != "v9.3.2"; then
      continue
    fi
    if test "$components" = "ompi" -a "$name" != "openmpi-2.0.3"; then
      continue
    fi
    if test "$components" = "other" -a "$name" = "openmpi-2.0.3"; then
      continue
    fi
    if test "$components" = "other" -a "$name" = "v9.3.2"; then
      continue
    fi
    cmake_opts="${extra_cmake_opts[$name]}"
    configure_opts="${extra_configure_opts[$name]}"
    cache_name="$name:$sha1:$make_opts:$configure_opts:$cmake_opts"
    if check_cache "$name"; then
       say "Using cached version of ${name}"
       continue
    fi
    export CC=gcc
    export CXX=g++
    if test "$name" = "v9.3.2"; then
      export VERBOSE=1
      rm -f ${prefix}/lib/libiberty.a
      export CC=gcc-4.8
      export CXX=g++-4.8
    fi
    if test "$name" = "openmpi-2.0.3"; then
      export CC=gcc-4.8
      export CXX=g++-4.8
    fi
    mkdir -p ${name}  || die "Failed to mkdir ${name}"
    (
      cd ${name} &&
      curl -L -O --insecure ${pkg} || die "Failed to download ${pkg}"
      tar --strip-components=1 -xf *.tar.gz || die "Failed to un-tar ${name}"
      if test -x configure; then
        ./configure --prefix=${prefix} \
                    $configure_opts  || head config.log
      elif test -f CMakeLists.txt; then
        if test "$name" = "v9.3.2"; then
          # travis boost 1.46.1 has a bug with shared_ptr, so force dyninst to build download/boost
          sed -i cmake/packages.cmake -e '125 s|#set (BOOST_MIN_VERSION 1.41.0)|set (BOOST_MIN_VERSION 1.50.0)|'
          grep BOOST_MIN cmake/packages.cmake
        fi
        mkdir build && cd build
        which cmake
        cmake -DCMAKE_INSTALL_PREFIX=${prefix} $cmake_opts ..
      fi
      if test "$name" = "openmpi-2.0.2"; then
        wget https://raw.githubusercontent.com/LLNL/STAT/develop/dyninst_patches/openmpi-2.0.2_reattach.patch
        patch -p1 < openmpi-2.0.2_reattach.patch
      fi
      make -j 8 PREFIX=${prefix} || make
      make PREFIX=${prefix} install
      if test "$name" = "launchmon-v1.0.2"; then
        pushd test/src
        echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
        export PATH=./:${prefix}/bin:$PATH
        which mpirun
        export LMON_FE_ENGINE_TIMEOUT=60
        cat test.launch_1
        ./test.launch_1
        sleep 60
        cat test.attach_1
        ./test.attach_1
        sleep 60
        echo 'LMON test done'
        popd
      fi
      if test "$name" = "libdwarf-20161124"; then
        pushd libdwarf
        mkdir -p ${prefix}/lib
        cp libdwarf.so ${prefix}/lib/libdwarf.so
        cp libdwarf.so.1 ${prefix}/lib/libdwarf.so.1
        mkdir -p ${prefix}/include
        cp libdwarf.h  ${prefix}/include/libdwarf.h
        cp dwarf.h ${prefix}/include/dwarf.h
        popd
      fi
      if test "$name" = "v9.3.2"; then
        pushd boost/src/boost
          # we will force dyninst to install its own boost b/c travis 1.46.1 has buggy shared_ptr
          ./b2 install
        popd
        ls
        if test -x libiberty/libiberty.a; then
          mkdir -p ${prefix}/lib
          cp libiberty/libiberty.a ${prefix}/lib/libiberty.a
        fi
      fi
    ) || die "Failed to build and install $name"
    add_cache "$name"
done


for url in $checkouts; do
    if test "$components" != "all" -a "$components" != "other"; then
      continue
    fi
    name=$(basename ${url} .git)
    sha1="${checkout_sha1[$name]}"
    make_opts="${extra_make_opts[$name]}"
    cmake_opts="${extra_cmake_opts[$name]}"
    configure_opts="${extra_configure_opts[$name]}"
    cache_name="$name:$sha1:$make_opts:$configure_opts:$cmake_opts"
    if check_cache "$cache_name"; then
       say "Using cached version of ${name}"
       continue
    fi
    git clone ${url} ${name} || die "Failed to clone ${url}"
    (
      cd ${name} || die "cd failed"
      if test -n "$sha1"; then
        git checkout $sha1
      fi

      # Do we need to create a Makefile?
      if ! test -f Makefile; then
        if ! test -f configure; then
          ./bootstrap
        fi
        if test -x configure; then
          ./configure --prefix=${prefix} \
                      --sysconfdir=${prefix}/etc \
                      $configure_opts || cat config.log
        elif test -f CMakeLists.txt; then
            mkdir build && cd build
            cmake -DCMAKE_INSTALL_PREFIX=${prefix} $cmake_opts ..
        fi
      fi
# The hack below is only needed in the dist:trusty travis env
# This will work around a ptrace Input/Output error when removing breakpoints
#      if test "$name" = "launchmon"; then
#        sed -i launchmon/src/linux/sdbg_linux_launchmon.cxx -e '2975 s|disable|//disable|'
#      fi
      make PREFIX=${prefix} $make_opts &&
      make PREFIX=${prefix} $make_opts install &&
      make check PREFIX=${prefix} $make_opts
      if test "$name" = "launchmon"; then
        pushd test/src
        echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
        export PATH=./:${prefix}/bin:$PATH
        which mpirun
        export LMON_FE_ENGINE_TIMEOUT=60
        cat test.launch_1
        ./test.launch_1
        sleep 60
        cat test.attach_1
        ./test.attach_1
        sleep 60
        echo 'LMON test done'
        popd
      fi
    ) || die "Failed to build and install $name"
    add_cache "$cache_name"
done

