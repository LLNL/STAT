# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\" 
AM_LDFLAGS = 

# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
include_HEADERS = STAT.h STAT_FrontEnd.h STAT_BackEnd.h STAT_timer.h
if ENABLE_STATBENCH
bin_PROGRAMS = STATbin STATD STATBenchbin STATBenchD
STATBenchbin_SOURCES = STATBench.C
STATBenchD_SOURCES = STATBenchD.C
STATBenchbin_LDFLAGS =
STATBenchD_LDFLAGS =
STATBenchbin_LDADD = libstatfe.la @FELIBS@ 
STATBenchD_LDADD = libstatbe.la @BELIBS@ 
else
bin_PROGRAMS = STATbin STATD 
endif
lib_LTLIBRARIES = libstatfe.la libstatbe.la 
STATbin_SOURCES = STAT.C STAT.h STAT_FrontEnd.h
STATD_SOURCES = STATD.C STAT.h STAT_BackEnd.h
STATbin_LDFLAGS = 
STATbin_LDADD = libstatfe.la @FELIBS@ 

STATD_LDFLAGS = 
STATD_LDADD = libstatbe.la @BELIBS@ 

libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h

pkglib_LTLIBRARIES = STAT_FilterDefinitions.la
pkglibdir = $(libdir)
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C
STAT_FilterDefinitions_la_LDFLAGS = -module -avoid-version
STAT_FilterDefinitions_la_LIBADD = @MWLIBS@

if ENABLE_GUI
BUILT_SOURCES = $(srcdir)/STAT_wrap.C
SWIG_SOURCES = STAT.i
pyexec_LTLIBRARIES = _STAT.la
python_PYTHON = STAT.py
_STAT_la_SOURCES = $(srcdir)/STAT_wrap.C $(SWIG_SOURCES)
_STAT_la_CPPFLAGS = -I./
_STAT_la_LDFLAGS = -module -avoid-version
_STAT_la_LIBADD = libstatfe.la @FELIBS@ 

$(srcdir)/STAT_wrap.C:$(SWIG_SOURCES) Makefile
	$(SWIG) -c++ -python -o $@ $<
endif    
