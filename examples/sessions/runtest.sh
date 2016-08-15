#!/bin/sh
#set -ex
#prefix="/nfs/tmp2/lee218/delete/spack-graphlib3/spack/opt/spack/linux-redhat6-x86_64/gcc-4.4.7/stat-3.0.0b-ak6btjinmvbf2ib7ihh7dwiowzlu2m3c"
#prefix="/nfs/tmp2/lee218/delete/stat-prefix"
#prefix="/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-3.0.0b"
prefix="/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test"
testnums="01 02 03"
testmodes="attach launch"
testnums="01"
testmodes="attach"
#testmodes="launch"

statcl_command="${prefix}/bin/stat-cl"
app_exe="${prefix}/share/STAT/examples/bin/dysect_test"
logmode=" -l FE -l BE -l CP "
topologymode=""
#topologymode=" -u 1-1-1 -p 8 -A -n 8 "

rm -rf test_results

for testnum in $testnums
do
  echo
  echo
  cmd="${prefix}/bin/dysectc test-$testnum.cpp"
  echo $cmd
  $cmd
  if test $? -ne 0
  then
    exit
  fi
  session_so="$PWD/libtest-$testnum.so"
  for testmode in $testmodes
  do
    echo
    echo
    echo test $testnum with mode $testmode
    date
    testdir="$PWD/test_results/$testnum$testmode"
    cmd="mkdir -p $testdir/logs"
    echo $cmd
    $cmd
    pushd $testdir
      if [ "$testmode" = "launch" ];
      then
        cmd="$statcl_command -X $session_so -L $testdir/logs $logmode $topologymode -C srun -n 8 $app_exe 20"
        echo $cmd
        $cmd
      else
        cmd="srun -n 8 $app_exe 20"
        echo $cmd
        $cmd &
        srun_pid=$!
        cmd="sleep 5"
        echo $cmd
        $cmd
        cmd="$statcl_command -X $session_so -L $testdir/logs $logmode $topologymode $srun_pid"
        echo $cmd
        $cmd
        wait $srun_pid
      fi
    popd
  done
done
