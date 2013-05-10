dist_bin_SCRIPTS = run_one_sec
if ENABLE_DYSECTAPI
bin_SCRIPTS = STAT stat-cl dysectc
else
bin_SCRIPTS = STAT stat-cl
endif
python_PYTHON = stat_merge_base.py STATmerge.py
if ENABLE_BGL
bin_SCRIPTS += core_stack_merge
python_PYTHON += bg_core_backtrace.py
endif
if ENABLE_GUI
python_PYTHON += STATGUI.py STATview.py xdot.py STAThelper.py
bin_SCRIPTS += STATGUI STATview stat-gui stat-view
endif
if ENABLE_STATBENCH
bin_SCRIPTS += STATBench stat-bench
endif
