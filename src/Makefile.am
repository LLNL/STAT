if ENABLE_DYSECTAPI
SUBDIRS = libDysectAPI
endif

# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\" 
AM_LDFLAGS = 

# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
include_HEADERS = STAT.h STAT_FrontEnd.h STAT_BackEnd.h STAT_timer.h STAT_GraphRoutines.h STAT_IncMRNet.h MRNetSymbolReader.h STAT_CircularLogs.h STAT_shared.h
if ENABLE_DYSECTAPI
nobase_include_HEADERS = DysectAPI/DysectAPIProcessMgr.h DysectAPI/Aggregates/Aggregate.h DysectAPI/Aggregates/AggregateFunction.h DysectAPI/Aggregates/Data.h DysectAPI/Aggregates/Packet.h DysectAPI/Aggregates/Location.h DysectAPI/Aggregates/StrAgg.h DysectAPI/Aggregates/DescVar.h DysectAPI/Aggregates/CmpAgg.h
stat_fe_ldadd_dysectapi = libDysectAPIFE.la @BELIBS@
stat_be_ldadd_dysectapi = libDysectAPIBE.la
stat_lib_ltlibraries_dysectapi = libDysectAPIFE.la libDysectAPIBE.la
stat_be_la_sources_dysectapi = DysectAPI/DysectAPIBE.C DysectAPI/DysectAPIBE.h DysectAPI/DysectAPI.h DysectAPI/DysectAPI.C DysectAPI/DysectAPIProcessMgr.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/AggregateBE.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationBE.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarBE.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggBE.C
stat_fe_la_sources_dysectapi = DysectAPI/DysectAPIFE.C DysectAPI/DysectAPIFE.h DysectAPI/DysectAPI.h DysectAPI/DysectAPI.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationFE.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarFE.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggFE.C
stat_fd_la_sources_dysectapi = DysectAPI/DysectAPIFilter.C DysectAPI/Aggregates/Aggregate.C DysectAPI/Aggregates/Data.C DysectAPI/Aggregates/StrAgg.C DysectAPI/Aggregates/Location.C DysectAPI/Aggregates/LocationFE.C DysectAPI/Aggregates/DescVar.C DysectAPI/Aggregates/DescVarCP.C DysectAPI/Aggregates/CmpAgg.C DysectAPI/Aggregates/CmpAggFE.C
stat_la_cppflags_dysectapi = -IlibDysectAPI/include -D__DYSECT_IS_FRONTEND
if ENABLE_STATBENCH
STATBenchD_CPPFLAGS=-IlibDysectAPI/include
STATBenchbin_CPPFLAGS=-IlibDysectAPI/include
endif
endif

if ENABLE_STATBENCH
stat_bin_programs_statbench = STATBenchbin STATBenchD
STATBenchbin_SOURCES = STATBench.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATBenchD_SOURCES = STATBenchD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATBenchbin_LDFLAGS = -Wl,-E
STATBenchD_LDFLAGS = -Wl,-E
STATBenchbin_LDADD = libstatfe.la $(stat_fe_ldadd_dysectapi) @FELIBS@
STATBenchD_LDADD = libstatbe.la $(stat_be_ldadd_dysectapi) @BELIBS@
endif

if ENABLE_GRAPHLIB20
stat_graphlib_sources = STAT_GraphRoutines.h STAT_GraphRoutines.C
endif

if ENABLE_FGFS
stat_be_la_sources_fgfs = MRNetSymbolReader.C
endif

bin_PROGRAMS = STATbin STATD $(stat_bin_programs_statbench)
lib_LTLIBRARIES = libstatfe.la libstatbe.la $(stat_lib_ltlibraries_dysectapi)

STATbin_SOURCES = STAT.C STAT.h STAT_FrontEnd.h STAT_GraphRoutines.h
STATD_SOURCES = STATD.C STAT.h STAT_BackEnd.h STAT_GraphRoutines.h
STATbin_LDFLAGS = -Wl,-E
STATbin_LDADD = libstatfe.la $(stat_fe_ldadd_dysectapi) @FELIBS@
STATD_LDFLAGS = -Wl,-E
STATD_LDADD = libstatbe.la $(stat_be_ldadd_dysectapi) @BELIBS@

stat_be_la_sources_base = STAT_BackEnd.C STAT_BackEnd.h STAT.h STAT_timer.h STAT_timer.C STAT_CircularLogs.C
stat_fe_la_sources_base = STAT_FrontEnd.C STAT_FrontEnd.h STAT.h STAT_timer.h STAT_timer.C

libstatbe_la_SOURCES = $(stat_be_la_sources_base) $(stat_graphlib_sources) $(stat_be_la_sources_dysectapi) $(stat_be_la_sources_fgfs)
libstatfe_la_SOURCES = $(stat_fe_la_sources_base) $(stat_graphlib_sources) $(stat_fe_la_sources_dysectapi)

if ENABLE_DYSECTAPI
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

libDysectAPIBE_la_SOURCES=$(dysect_sources) libDysectAPI/src/location_be.cpp libDysectAPI/src/backend.cpp libDysectAPI/src/group_be.cpp libDysectAPI/src/probe_be.cpp libDysectAPI/src/expr_be.cpp libDysectAPI/src/symbol.cpp libDysectAPI/src/safe_timer.cpp libDysectAPI/src/action_be.cpp libDysectAPI/src/event_be.cpp libDysectAPI/src/domain_be.cpp libDysectAPI/src/condition_be.cpp libDysectAPI/src/time_be.cpp
libDysectAPIBE_la_CPPFLAGS=-g -O2 -fPIC $(dysect_includes) -D__DYSECT_IS_BACKEND 
libDysectAPIBE_la_LDFLAGS=--export-dynamic -lrt
endif

pkglib_LTLIBRARIES = STAT_FilterDefinitions.la
pkglibdir = $(libdir)

STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C $(stat_graphlib_sources) $(stat_fd_la_sources_dysectapi)
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
_STAT_la_CPPFLAGS = -I./ $(stat_la_cppflags_dysectapi)
_STAT_la_LDFLAGS = -module -avoid-version
_STAT_la_LIBADD = libstatfe.la $(stat_fe_ldadd_dysectapi) @FELIBS@

STAT_wrap.C:$(SWIG_SOURCES) Makefile
	$(SWIG) -c++ -python -o $@ $<
endif    

clean-local:
	-rm -rf STAT_wrap.C STAT.py
