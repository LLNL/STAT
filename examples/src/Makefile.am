MPICC = @MPICC@
CC = @MPICC@
MPICXX = @MPICXX@
CXX = @MPICXX@
MPIFC = @MPIFC@
FC = @MPIFC@
MPI77 = @MPIFC@
F77 = @MPIFC@
if ENABLE_EXAMPLES
examplesdir = $(pkgdatadir)/examples/bin
examples_PROGRAMS = hw mpi_ringtopo mpi_ringtopo_defunct to_test rank_test template_test mpi_ringtopo2 dysect_test
endif
hw_SOURCES = hw.F
hw_LIBTOOLFLAGS = --tag FC
mpi_ringtopo_SOURCES = mpi_ringtopo.c
rank_test_SOURCES = rank_test.c
mpi_ringtopo_defunct_SOURCES = mpi_ringtopo_defunct.c
to_test_SOURCES = to_test.c
template_test_SOURCES = template_test.cpp
mpi_ringtopo2_SOURCES = mpi_ringtopo2.cpp
dysect_test_SOURCES = dysect_test.cpp

hw(EXEEXT): hw.F Makefile
	$(MPIFC) -gdwarf-2 -o $@ $<
mpi_ringtopo(EXEEXT): mpi_ringtopo.c Makefile
	$(MPICC) -g -o $@ $<
rank_test(EXEEXT): rank_test.c Makefile
	$(MPICC) -g -gdwarf-2 -o $@ $<
mpi_ringtopo_defunct(EXEEXT): mpi_ringtopo_defunct.c Makefile
	$(MPICC) -g -gdwarf-2 -o $@ $<
to_test(EXEEXT): to_test.c Makefile
	$(MPICC) -g -gdwarf-2 -o $@ $<
template_test(EXEEXT): template_test.cpp Makefile
	$(MPICXX) -g -gdwarf-2 -o $@ $<
mpi_ringtopo2(EXEEXT): mpi_ringtopo2.cpp Makefile
	$(MPICXX) -g -gdwarf-2 -O0 -o $@ $<
dysect_test(EXEEXT): dysect_test.cpp Makefile
	$(MPICXX) -g -gdwarf-2 -O0 -o $@ $<
