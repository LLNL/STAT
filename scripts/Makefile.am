dist_bin_SCRIPTS = run_one_sec
if ENABLE_DYSECTAPI
bin_SCRIPTS = STAT stat-cl stat-script stat-core-merger dysectc
else
bin_SCRIPTS = STAT stat-cl stat-script stat-core-merger
endif
python_PYTHON = stat_merge_base.py STATmerge.py core_file_merger.py bg_core_backtrace.py
if ENABLE_GUI
python_PYTHON += STATGUI.py STATview.py xdot.py STAThelper.py
bin_SCRIPTS += STATGUI STATview stat-gui stat-view
endif
if ENABLE_STATBENCH
bin_SCRIPTS += STATBench stat-bench
endif
