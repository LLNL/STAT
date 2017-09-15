dist_bin_SCRIPTS = run_one_sec
if ENABLE_DYSECTAPI
bin_SCRIPTS = STAT stat-cl stat-script stat-core-merger attach-helper dysectc
else
bin_SCRIPTS = STAT stat-cl stat-script stat-core-merger attach-helper
endif
python_PYTHON = STATmain.py stat_merge_base.py STATmerge.py core_file_merger.py bg_core_backtrace.py attach_helper.py
if ENABLE_CUDAGDB
python_PYTHON += stat_cuda_gdb.py cuda_gdb.py gdb.py
endif
if ENABLE_GUI
python_PYTHON += STATGUI.py STATview.py xdot.py STAThelper.py
if ENABLE_DYSECTAPI
python_PYTHON += DysectView.py
bin_SCRIPTS += dysect-view
endif
bin_SCRIPTS += STATGUI STATview stat-gui stat-view
endif
if ENABLE_STATBENCH
bin_SCRIPTS += STATBench stat-bench
endif
