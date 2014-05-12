CC = @CC@
MPICC = @MPICC@
F77 = @F77@
MPIF77 = @MPIF77@
examplesdir = $(pkgdatadir)/examples/bin
examples_PROGRAMS = hw mpi_ringtopo mpi_ringtopo_defunct to_test rank_test template_test mpi_ringtopo2
hw_SOURCES = hw.F
mpi_ringtopo_SOURCES = mpi_ringtopo.c
rank_test_SOURCES = rank_test.c
mpi_ringtopo_defunct_SOURCES = mpi_ringtopo_defunct.c
to_test_SOURCES = to_test.c
template_test_SOURCES = template_test.cpp
mpi_ringtopo2_SOURCES = mpi_ringtopo2.cpp

hw: hw.F Makefile
	$(MPIF77) -gdwarf-2 -o $@ $<
mpi_ringtopo: mpi_ringtopo.c Makefile
	$(MPICC) -g -o $@ $<
rank_test: rank_test.c Makefile
	$(MPICC) -g -gdwarf-2 -o $@ $<
mpi_ringtopo_defunct: mpi_ringtopo_defunct.c Makefile
	$(MPICC) -g -gdwarf-2  -o $@ $<
to_test: to_test.c Makefile
	$(MPICC) -g -gdwarf-2 -o $@ $<
template_test: template_test.cpp Makefile
	$(MPICXX) -g -gdwarf-2 -o $@ $<
mpi_ringtopo2: mpi_ringtopo2.cpp Makefile
	$(MPICXX) -g -gdwarf-2 -O0 -o $@ $<
