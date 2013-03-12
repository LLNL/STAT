# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\" 
AM_LDFLAGS = 

# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
include_HEADERS = STAT.h STAT_FrontEnd.h STAT_BackEnd.h STAT_timer.h STAT_GraphRoutines.h STAT_IncMRNet.h MRNetSymbolReader.h STAT_CircularLogs.h
if ENABLE_STATBENCH
bin_PROGRAMS = STATbin STATD STATBenchbin STATBenchD
STATBenchbin_SOURCES = STATBench.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATBenchD_SOURCES = STATBenchD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATBenchbin_LDFLAGS = -Wl,-E
STATBenchD_LDFLAGS = -Wl,-E
STATBenchbin_LDADD = libstatfe.la @FELIBS@ 
STATBenchD_LDADD = libstatbe.la @BELIBS@ 
else
bin_PROGRAMS = STATbin STATD 
endif
lib_LTLIBRARIES = libstatfe.la libstatbe.la 
STATbin_SOURCES = STAT.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATD_SOURCES = STATD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATbin_LDFLAGS = -Wl,-E
STATbin_LDADD = libstatfe.la @FELIBS@ 

STATD_LDFLAGS = -Wl,-E
STATD_LDADD = libstatbe.la @BELIBS@ 

if ENABLE_GRAPHLIB20
libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_GraphRoutines.h STAT_GraphRoutines.C
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_GraphRoutines.h STAT_GraphRoutines.C STAT_CircularLogs.C
else
libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_CircularLogs.C
endif

pkglib_LTLIBRARIES = STAT_FilterDefinitions.la
pkglibdir = $(libdir)
if ENABLE_GRAPHLIB20
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C STAT_GraphRoutines.h STAT_GraphRoutines.C
else
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C
endif
STAT_FilterDefinitions_la_LDFLAGS = -module -avoid-version
STAT_FilterDefinitions_la_LIBADD = @MWLIBS@

pyexec_LTLIBRARIES = _STATmerge.la
_STATmerge_la_SOURCES = STAT_merge.C STAT_GraphRoutines.C
_STATmerge_la_CPPFLAGS = -I./ 
_STATmerge_la_LDFLAGS = -module -avoid-version
_STATmerge_la_LIBADD = -llnlgraph

if ENABLE_GUI
BUILT_SOURCES = STAT_wrap.C
SWIG_SOURCES = STAT.i
pyexec_LTLIBRARIES += _STAT.la 
python_PYTHON = STAT.py
_STAT_la_SOURCES = STAT_wrap.C $(SWIG_SOURCES)
_STAT_la_CPPFLAGS = -I./
_STAT_la_LDFLAGS = -module -avoid-version
_STAT_la_LIBADD = libstatfe.la @FELIBS@

STAT_wrap.C:$(SWIG_SOURCES) Makefile
	$(SWIG) -c++ -python -o $@ $<
endif    

clean-local:
	-rm -rf STAT_wrap.C STAT.py
