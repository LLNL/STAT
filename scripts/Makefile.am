dist_bin_SCRIPTS = run_one_sec
bin_SCRIPTS = STAT stat-cl
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
