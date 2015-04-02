# what flags you want to pass to the C compiler & linker
AM_CXXFLAGS = -DSTAT_PREFIX=\"$(prefix)\"
AM_LDFLAGS =
ACLOCAL_AMFLAGS = -I m4
AM_LFLAGS = --header-file=expr-scanner.h
AM_YFLAGS = -d
BUILT_SOURCES = expr-parser.h expr-scanner.h

lib_LTLIBRARIES = libDysectFE.la libDysectBE.la

libdysect_includes = -I$(abs_top_srcdir)/src -I$(abs_top_srcdir)/src/dysect -I$(abs_top_srcdir)/src/dysect/libDysectAPI/include
libdysect_sources = action.cpp \
                    condition.cpp \
                    domain.cpp \
                    expr-parser.yy \
                    expr-scanner.ll \
                    event.cpp \
                    probe.cpp \
                    parser.cpp \
                    probe_tree.cpp \
                    default_probes.cpp \
                    group.cpp \
                    err.cpp \
                    expr.cpp


libDysectFE_la_SOURCES = $(libdysect_sources) location_fe.cpp frontend.cpp group_fe.cpp probe_fe.cpp expr_fe.cpp action_fe.cpp event_fe.cpp domain_fe.cpp time_fe.cpp
libDysectFE_la_CPPFLAGS = -g -O2 -fPIC $(libdysect_includes) -D__DYSECT_IS_FRONTEND
libDysectFE_la_LDFLAGS = --export-dynamic

libDysectBE_la_SOURCES = $(libdysect_sources) location_be.cpp backend.cpp group_be.cpp probe_be.cpp expr_be.cpp symbol.cpp safe_timer.cpp action_be.cpp event_be.cpp domain_be.cpp condition_be.cpp time_be.cpp
libDysectBE_la_CPPFLAGS = -g -O2 -fPIC $(libdysect_includes) -D__DYSECT_IS_BACKEND
libDysectBE_la_LDFLAGS = --export-dynamic -lrt

# hack to work around automake's ylwrap bug (not acepting --header-file option)
expr-scanner.cc:expr-scanner.ll Makefile
	$(LEX) $(LFLAGS) $(AM_LFLAGS) -o $@ $<

expr-scanner.h:expr-scanner.cc

expr-parser.h:expr-scanner.cc
	if [ -f expr-parser.hh ]; then \
		cp $< expr-parser.hh; \
	fi
clean-local:
	-rm -rf expr-scanner.h expr-scanner.cc expr-parser.cc expr-parser.h
