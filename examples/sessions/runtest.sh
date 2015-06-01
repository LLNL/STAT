#!/bin/sh
#set -ex

#testnums="01 02 03"
#testmodes="attach launch"
testnums="01"
testmodes="launch"

statcl_command="/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test/bin/stat-cl"
#statcl_command="/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test/bin/STAT-inspxe"
#export STAT_DAEMON_PATH="/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test/bin/STATD-inspxe"
app_exe="/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test/share/STAT/examples/bin/dysect_test"
logmode=" -l FE -l BE -l CP "
topologymode=" -u 1-1-1 -p 8 -A -n 8 "

rm -rf test_results

for testnum in $testnums
do
  echo
  echo
  cmd="/collab/usr/global/tools/stat/chaos_5_x86_64_ib/stat-test/bin/dysectc test-$testnum.cpp"
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
        cmd="$statcl_command -X $session_so -L $testdir/logs $logmode $topologymode $!"
        echo $cmd
        $cmd
        wait $srun_pid
      fi
    popd
  done
done
