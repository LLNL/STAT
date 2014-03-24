# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\"
AM_LDFLAGS =

bin_PROGRAMS = STATbin STATD
# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
include_HEADERS = STAT.h STAT_FrontEnd.h STAT_BackEnd.h STAT_timer.h STAT_GraphRoutines.h STAT_IncMRNet.h MRNetSymbolReader.h STAT_CircularLogs.h
if ENABLE_STATBENCH
bin_PROGRAMS += STATBenchbin STATBenchD
STATBenchbin_SOURCES = STATBench.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATBenchD_SOURCES = STATBenchD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATBenchbin_LDFLAGS = -Wl,-E
STATBenchD_LDFLAGS = -Wl,-E
STATBenchbin_LDADD = libstatfe.la @FELIBS@
STATBenchD_LDADD = libstatbe.la @BELIBS@
endif
lib_LTLIBRARIES = libstatfe.la libstatbe.la
STATbin_SOURCES = STAT.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATD_SOURCES = STATD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATbin_LDFLAGS = -Wl,-E
STATbin_LDADD = libstatfe.la @FELIBS@

STATD_LDFLAGS = -Wl,-E
STATD_LDADD = libstatbe.la @BELIBS@

libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_CircularLogs.C
if ENABLE_GRAPHLIB20
libstatfe_la_SOURCES += STAT_GraphRoutines.h STAT_GraphRoutines.C
libstatbe_la_SOURCES += STAT_GraphRoutines.h STAT_GraphRoutines.C
endif

pkglib_LTLIBRARIES = STAT_FilterDefinitions.la
pkglibdir = $(libdir)
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C
if ENABLE_GRAPHLIB20
STAT_FilterDefinitions_la_SOURCES += STAT_GraphRoutines.h STAT_GraphRoutines.C
endif
STAT_FilterDefinitions_la_LDFLAGS = -module -avoid-version
STAT_FilterDefinitions_la_LIBADD = @MWLIBS@

pyexec_LTLIBRARIES = _STATmerge.la
_STATmerge_la_SOURCES = STAT_merge.C STAT_GraphRoutines.C
_STATmerge_la_CPPFLAGS = -I./
_STATmerge_la_LDFLAGS = -module -avoid-version
_STATmerge_la_LIBADD = -llnlgraph

python_PYTHON =
if ENABLE_GUI
GUI_SWIG_SOURCES = STAT.i
pyexec_LTLIBRARIES += _STAT.la
python_PYTHON += STAT.py
_STAT_la_SOURCES = STAT_wrap.C $(GUI_SWIG_SOURCES)
_STAT_la_CPPFLAGS = -I./
_STAT_la_LDFLAGS = -module -avoid-version
_STAT_la_LIBADD = libstatfe.la @FELIBS@

STAT_wrap.C:$(GUI_SWIG_SOURCES) Makefile
	$(SWIG) -c++ -python -o $@ $<

STAT.py:$(GUI_SWIG_SOURCES) Makefile
	$(MAKE) $(AM_MAKEFLAGS STAT_wrap.C
endif

if ENABLE_TO
TO_SWIG_SOURCES = to.i
pyexec_LTLIBRARIES += _tomod.la
python_PYTHON += tomod.py
_tomod_la_SOURCES = to_wrap.C to.C to.h $(TO_SWIG_SOURCES)
_tomod_la_CPPFLAGS = -I./ $(BOOST_INCLUDE) $(ROSE_INCLUDE) $(TOAPI_INCLUDE)
_tomod_la_LDFLAGS = -module -avoid-version -L$(LIBBOOSTDIR) -L$(LIBJAVADIR) -L$(LIBROSEDIR) -L$(LIBTOAPIDIR) $(LIBTOAPI) -Wl,-rpath=$(LIBBOOSTDIR) -Wl,-rpath=$(LIBROSEDIR) -Wl,-rpath=$(LIBTOAPIDIR) -Wl,-rpath=$(LIBJAVADIR)

to_wrap.C:$(TO_SWIG_SOURCES) Makefile
	$(SWIG) -c++ -python -o $@ $<

tomod.py:$(TO_SWIG_SOURCES) Makefile
	$(MAKE) $(AM_MAKEFLAGS to_wrap.C
endif

clean-local:
	-rm -rf STAT_wrap.C STAT.py to_wrap.C tomod.py
