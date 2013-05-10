if ENABLE_DYSECTAPI
SUBDIRS = libDysectAPI
endif

# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\" 
AM_LDFLAGS = 

# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
if ENABLE_DYSECTAPI
include_HEADERS = STAT.h STAT_FrontEnd.h STAT_BackEnd.h STAT_timer.h STAT_GraphRoutines.h STAT_IncMRNet.h MRNetSymbolReader.h STAT_CircularLogs.h STAT_shared.h
else
include_HEADERS = STAT.h STAT_FrontEnd.h STAT_BackEnd.h STAT_timer.h STAT_GraphRoutines.h STAT_IncMRNet.h MRNetSymbolReader.h STAT_CircularLogs.h STAT_shared.h
endif
if ENABLE_DYSECTAPI
nobase_include_HEADERS = DysectAPI/DysectAPIProcessMgr.h DysectAPI/Aggregates/Aggregate.h DysectAPI/Aggregates/AggregateFunction.h DysectAPI/Aggregates/Data.h DysectAPI/Aggregates/Packet.h DysectAPI/Aggregates/Location.h DysectAPI/Aggregates/StrAgg.h DysectAPI/Aggregates/DescVar.h DysectAPI/Aggregates/CmpAgg.h
endif
if ENABLE_STATBENCH
bin_PROGRAMS = STATbin STATD STATBenchbin STATBenchD
STATBenchbin_SOURCES = STATBench.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATBenchD_SOURCES = STATBenchD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATBenchbin_LDFLAGS = -Wl,-E
STATBenchD_LDFLAGS = -Wl,-E
if ENABLE_DYSECTAPI
STATBenchbin_LDADD = libstatfe.la libDysectAPIFE.la @FELIBS@ @BELIBS@
STATBenchD_LDADD = libstatbe.la libDysectAPIBE.la @BELIBS@ 
else
STATBenchbin_LDADD = libstatfe.la @FELIBS@ 
STATBenchD_LDADD = libstatbe.la @BELIBS@ 
endif
else
bin_PROGRAMS = STATbin STATD 
endif
if ENABLE_DYSECTAPI
lib_LTLIBRARIES = libstatfe.la libstatbe.la libDysectAPIFE.la libDysectAPIBE.la
else
lib_LTLIBRARIES = libstatfe.la libstatbe.la 
endif
STATbin_SOURCES = STAT.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATD_SOURCES = STATD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATbin_LDFLAGS = -Wl,-E
if ENABLE_DYSECTAPI
STATbin_LDADD = libstatfe.la libDysectAPIFE.la @FELIBS@ @BELIBS@
else
STATbin_LDADD = libstatfe.la @FELIBS@ 
endif

STATD_LDFLAGS = -Wl,-E
if ENABLE_DYSECTAPI
STATD_LDADD = libstatbe.la libDysectAPIBE.la @BELIBS@ 
else
STATD_LDADD = libstatbe.la @BELIBS@ 
endif

if ENABLE_GRAPHLIB20
if ENABLE_DYSECTAPI
libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_GraphRoutines.h STAT_GraphRoutines.C DysectAPI/DysectAPIFE.C DysectAPI/DysectAPIFE.h DysectAPI/DysectAPI.h DysectAPI/DysectAPI.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationFE.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarFE.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggFE.C
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_GraphRoutines.h STAT_GraphRoutines.C STAT_CircularLogs.C MRNetSymbolReader.C DysectAPI/DysectAPIBE.C DysectAPI/DysectAPIBE.h DysectAPI/DysectAPI.h DysectAPI/DysectAPI.C DysectAPI/DysectAPIProcessMgr.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/AggregateBE.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationBE.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarBE.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggBE.C
else
libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_GraphRoutines.h STAT_GraphRoutines.C
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_GraphRoutines.h STAT_GraphRoutines.C STAT_CircularLogs.C MRNetSymbolReader.C
endif
else
if ENABLE_DYSECTAPI
libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C DysectAPI/DysectAPIFE.C DysectAPI/DysectAPIFE.h DysectAPI/DysectAPI.h DysectAPI/DysectAPI.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationFE.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarFE.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggFE.C
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C DysectAPI/DysectAPIBE.C DysectAPI/DysectAPIProcessMgr.C DysectAPI/DysectAPIBE.h DysectAPI/DysectAPI.h DysectAPI/DysectAPI.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/AggregateBE.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarBE.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggBE.C
else
libstatfe_la_SOURCES = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C
libstatbe_la_SOURCES = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_CircularLogs.C
endif
endif

if ENABLE_DYSECTAPI
###### DysectAPI ######
if ENABLE_STATBENCH
STATBenchD_CPPFLAGS=-IlibDysectAPI/include
STATBenchbin_CPPFLAGS=-IlibDysectAPI/include
endif
libstatbe_la_CPPFLAGS=-IlibDysectAPI/include
libstatfe_la_CPPFLAGS=-IlibDysectAPI/include
STATD_CPPFLAGS=-IlibDysectAPI/include
STATbin_CPPFLAGS=-IlibDysectAPI/include

dysect_sources = 	libDysectAPI/src/action.cpp \
									libDysectAPI/src/condition.cpp \
									libDysectAPI/src/domain.cpp \
									libDysectAPI/src/event.cpp \
									libDysectAPI/src/probe.cpp \
									libDysectAPI/src/parser.cpp \
									libDysectAPI/src/probe_tree.cpp \
									libDysectAPI/src/default_probes.cpp \
									libDysectAPI/src/group.cpp \
									libDysectAPI/src/err.cpp \
									libDysectAPI/src/expr.cpp \
									libDysectAPI/src/expr-scanner.ll \
									libDysectAPI/src/expr-parser.yy

dysect_includes = -IlibDysectAPI/include 

libDysectAPIFE_la_SOURCES=$(dysect_sources) libDysectAPI/src/location_fe.cpp libDysectAPI/src/frontend.cpp libDysectAPI/src/group_fe.cpp libDysectAPI/src/probe_fe.cpp libDysectAPI/src/expr_fe.cpp libDysectAPI/src/action_fe.cpp libDysectAPI/src/event_fe.cpp libDysectAPI/src/domain_fe.cpp libDysectAPI/src/time_fe.cpp 
libDysectAPIFE_la_CPPFLAGS=-g -O2 -fPIC $(dysect_includes) -D__DYSECT_IS_FRONTEND
libDysectAPIFE_la_LDFLAGS=--export-dynamic 

libDysectAPIBE_la_SOURCES=$(dysect_sources) libDysectAPI/src/location_be.cpp libDysectAPI/src/backend.cpp libDysectAPI/src/group_be.cpp libDysectAPI/src/probe_be.cpp libDysectAPI/src/expr_be.cpp libDysectAPI/src/symbol.cpp libDysectAPI/src/safe_timer.cpp libDysectAPI/src/action_be.cpp libDysectAPI/src/event_be.cpp libDysectAPI/src/domain_be.cpp libDysectAPI/src/condition_be.cpp libDysectAPI/src/time_fe.cpp
libDysectAPIBE_la_CPPFLAGS=-g -O2 -fPIC $(dysect_includes) -D__DYSECT_IS_BACKEND 
libDysectAPIBE_la_LDFLAGS=--export-dynamic -lrt
#######################
endif

pkglib_LTLIBRARIES = STAT_FilterDefinitions.la
pkglibdir = $(libdir)
if ENABLE_GRAPHLIB20
if ENABLE_DYSECTAPI
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C STAT_GraphRoutines.h STAT_GraphRoutines.C DysectAPI/DysectAPIFilter.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationFE.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarCP.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggFE.C
else
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C STAT_GraphRoutines.h STAT_GraphRoutines.C
endif
else
if ENABLE_DYSECTAPI
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C DysectAPI/DysectAPIFilter.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationFE.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarCP.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggFE.C
else
STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C
endif
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
if ENABLE_DYSECTAPI
_STAT_la_CPPFLAGS = -I./ -IlibDysectAPI/include -D__DYSECT_IS_FRONTEND
else
_STAT_la_CPPFLAGS = -I./
endif
_STAT_la_LDFLAGS = -module -avoid-version
if ENABLE_DYSECTAPI
_STAT_la_LIBADD = libstatfe.la libDysectAPIFE.la @FELIBS@ @BELIBS@
else
_STAT_la_LIBADD = libstatfe.la @FELIBS@
endif

STAT_wrap.C:$(SWIG_SOURCES) Makefile
	$(SWIG) -c++ -python -o $@ $<
endif    

clean-local:
	-rm -rf STAT_wrap.C STAT.py
