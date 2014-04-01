# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\"
AM_LDFLAGS =

lib_LTLIBRARIES = libDysectAPIFE.la libDysectAPIBE.la libDysectAPIFD.la

dysectapi_includes = -I$(abs_top_srcdir)/src -I$(abs_top_srcdir)/src/dysect -I$(abs_top_srcdir)/src/dysect/libDysectAPI/include
dysectapi_sources = DysectAPI.h DysectAPI.C
dysectapi_aggregate_sources = Aggregates/Data.C Aggregates/Aggregate.C Aggregates/Location.C Aggregates/CmpAgg.C Aggregates/DescVar.C Aggregates/StrAgg.C

libDysectAPIFE_la_SOURCES = DysectAPIFE.C DysectAPIFE.h Aggregates/LocationFE.C Aggregates/DescVarFE.C Aggregates/CmpAggFE.C $(dysectapi_sources) $(dysectapi_aggregate_sources)
libDysectAPIFE_la_CPPFLAGS = -g -O2 -fPIC $(dysectapi_includes) -D__DYSECT_IS_FRONTEND
libDysectAPIFE_la_LDFLAGS = --export-dynamic

libDysectAPIBE_la_SOURCES = DysectAPIBE.C DysectAPIBE.h DysectAPIProcessMgr.C Aggregates/AggregateBE.C Aggregates/LocationBE.C Aggregates/DescVarBE.C Aggregates/CmpAggBE.C $(dysectapi_sources) $(dysectapi_aggregate_sources)
libDysectAPIBE_la_CPPFLAGS = -g -O2 -fPIC $(dysectapi_includes) -D__DYSECT_IS_BACKEND
libDysectAPIBE_la_LDFLAGS = --export-dynamic -lrt

libDysectAPIFD_la_SOURCES = DysectAPIFilter.C DysectAPIFilter.h Aggregates/LocationFE.C Aggregates/DescVarCP.C Aggregates/CmpAggFE.C $(dysectapi_aggregate_sources)
libDysectAPIFD_la_CPPFLAGS = -g -O2 -fPIC $(dysectapi_includes)
libDysectAPIFD_la_LDFLAGS = --export-dynamic -lrt
