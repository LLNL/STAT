if ENABLE_DYSECTAPI
SUBDIRS = dysect
endif

# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\"
AM_LDFLAGS =

include_HEADERS = STAT.h STAT_FrontEnd.h STAT_BackEnd.h STAT_timer.h STAT_GraphRoutines.h STAT_IncMRNet.h MRNetSymbolReader.h STAT_CircularLogs.h STAT_shared.h
if ENABLE_DYSECTAPI
stat_fe_ldadd_dysectapi = libDysectAPIFE.la @BELIBS@
stat_be_ldadd_dysectapi = libDysectAPIBE.la
stat_lib_ltlibraries_dysectapi = libDysectAPIFE.la libDysectAPIBE.la
stat_be_la_sources_dysectapi = dysect/DysectAPI/DysectAPIBE.C dysect/DysectAPI/DysectAPIBE.h dysect/DysectAPI/DysectAPI.h dysect/DysectAPI/DysectAPI.C dysect/DysectAPI/DysectAPIProcessMgr.C dysect/DysectAPI/Aggregates/Aggregate.C dysect/DysectAPI/Aggregates/Data.C dysect/DysectAPI/Aggregates/AggregateBE.C dysect/DysectAPI/Aggregates/Location.C dysect/DysectAPI/Aggregates/LocationBE.C dysect/DysectAPI/Aggregates/StrAgg.C dysect/DysectAPI/Aggregates/DescVar.C dysect/DysectAPI/Aggregates/DescVarBE.C dysect/DysectAPI/Aggregates/CmpAgg.C dysect/DysectAPI/Aggregates/CmpAggBE.C
stat_fe_la_sources_dysectapi = dysect/DysectAPI/DysectAPIFE.C dysect/DysectAPI/DysectAPIFE.h dysect/DysectAPI/DysectAPI.h dysect/DysectAPI/DysectAPI.C dysect/DysectAPI/Aggregates/Data.C dysect/DysectAPI/Aggregates/Aggregate.C dysect/DysectAPI/Aggregates/Location.C dysect/DysectAPI/Aggregates/LocationFE.C dysect/DysectAPI/Aggregates/StrAgg.C dysect/DysectAPI/Aggregates/DescVar.C dysect/DysectAPI/Aggregates/DescVarFE.C dysect/DysectAPI/Aggregates/CmpAgg.C dysect/DysectAPI/Aggregates/CmpAggFE.C
stat_fd_la_sources_dysectapi = dysect/DysectAPI/DysectAPIFilter.C dysect/DysectAPI/Aggregates/Aggregate.C dysect/DysectAPI/Aggregates/Data.C dysect/DysectAPI/Aggregates/StrAgg.C dysect/DysectAPI/Aggregates/Location.C dysect/DysectAPI/Aggregates/LocationFE.C dysect/DysectAPI/Aggregates/DescVar.C dysect/DysectAPI/Aggregates/DescVarCP.C dysect/DysectAPI/Aggregates/CmpAgg.C dysect/DysectAPI/Aggregates/CmpAggFE.C
stat_la_cppflags_dysectapi = -Idysect -Idysect/libDysectAPI/include -D__DYSECT_IS_FRONTEND
if ENABLE_STATBENCH
STATBenchD_CPPFLAGS=-Idysect/libDysectAPI/include -Idysect
STATBenchbin_CPPFLAGS=-Idysect/libDysectAPI/include -Idysect
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
libstatbe_la_CPPFLAGS=-Idysect/libDysectAPI/include -Idysect
libstatfe_la_CPPFLAGS=-Idysect/libDysectAPI/include -Idysect
STATD_CPPFLAGS=-Idysect/libDysectAPI/include -Idysect
STATbin_CPPFLAGS=-Idysect/libDysectAPI/include -Idysect

dysect_sources = 	dysect/libDysectAPI/src/action.cpp \
									dysect/libDysectAPI/src/condition.cpp \
									dysect/libDysectAPI/src/domain.cpp \
									dysect/libDysectAPI/src/event.cpp \
									dysect/libDysectAPI/src/probe.cpp \
									dysect/libDysectAPI/src/parser.cpp \
									dysect/libDysectAPI/src/probe_tree.cpp \
									dysect/libDysectAPI/src/default_probes.cpp \
									dysect/libDysectAPI/src/group.cpp \
									dysect/libDysectAPI/src/err.cpp \
									dysect/libDysectAPI/src/expr.cpp \
									dysect/libDysectAPI/src/expr-scanner.ll \
									dysect/libDysectAPI/src/expr-parser.yy

dysect_includes = -Idysect -Idysect/libDysectAPI/include 

libDysectAPIFE_la_SOURCES=$(dysect_sources) dysect/libDysectAPI/src/location_fe.cpp dysect/libDysectAPI/src/frontend.cpp dysect/libDysectAPI/src/group_fe.cpp dysect/libDysectAPI/src/probe_fe.cpp dysect/libDysectAPI/src/expr_fe.cpp dysect/libDysectAPI/src/action_fe.cpp dysect/libDysectAPI/src/event_fe.cpp dysect/libDysectAPI/src/domain_fe.cpp dysect/libDysectAPI/src/time_fe.cpp 
libDysectAPIFE_la_CPPFLAGS=-g -O2 -fPIC $(dysect_includes) -D__DYSECT_IS_FRONTEND
libDysectAPIFE_la_LDFLAGS=--export-dynamic 

libDysectAPIBE_la_SOURCES=$(dysect_sources) dysect/libDysectAPI/src/location_be.cpp dysect/libDysectAPI/src/backend.cpp dysect/libDysectAPI/src/group_be.cpp dysect/libDysectAPI/src/probe_be.cpp dysect/libDysectAPI/src/expr_be.cpp dysect/libDysectAPI/src/symbol.cpp dysect/libDysectAPI/src/safe_timer.cpp dysect/libDysectAPI/src/action_be.cpp dysect/libDysectAPI/src/event_be.cpp dysect/libDysectAPI/src/domain_be.cpp dysect/libDysectAPI/src/condition_be.cpp dysect/libDysectAPI/src/time_be.cpp
libDysectAPIBE_la_CPPFLAGS=-g -O2 -fPIC $(dysect_includes) -D__DYSECT_IS_BACKEND 
libDysectAPIBE_la_LDFLAGS=--export-dynamic -lrt
endif

pkglib_LTLIBRARIES = STAT_FilterDefinitions.la
pkglibdir = $(libdir)

STAT_FilterDefinitions_la_SOURCES = STAT_FilterDefinitions.C $(stat_graphlib_sources) $(stat_fd_la_sources_dysectapi)
STAT_FilterDefinitions_la_LDFLAGS = -module -avoid-version
STAT_FilterDefinitions_la_LIBADD = @MWLIBS@
STAT_FilterDefinitions_la_CPPFLAGS = $(dysect_includes)

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
_STAT_la_CPPFLAGS = -I./ $(dysect_includes) -D__DYSECT_IS_FRONTEND
_STAT_la_LDFLAGS = -module -avoid-version
_STAT_la_LIBADD = libstatfe.la $(stat_fe_ldadd_dysectapi) @FELIBS@

STAT_wrap.C:$(GUI_SWIG_SOURCES) Makefile
	$(SWIG) -c++ -python -o $@ $<

STAT.py:$(GUI_SWIG_SOURCES) Makefile
	$(MAKE) $(AM_MAKEFLAGS) STAT_wrap.C
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
	$(MAKE) $(AM_MAKEFLAGS) to_wrap.C
endif

clean-local:
	-rm -rf STAT_wrap.C STAT.py to_wrap.C tomod.py
