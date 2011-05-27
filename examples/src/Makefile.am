CC = @CC@
MPICC = @MPICC@
F77 = @F77@
MPIF77 = @MPIF77@
examplesdir = $(pkgdatadir)/examples/bin
examples_PROGRAMS = hw mpi_ringtopo mpi_ringtopo_defunct to_test rank_test
hw_SOURCES = hw.F
mpi_ringtopo_SOURCES = mpi_ringtopo.c
rank_test_SOURCES = rank_test.c
mpi_ringtopo_defunct_SOURCES = mpi_ringtopo_defunct.c
to_test_SOURCES = to_test.c

hw: hw.F Makefile
	$(MPIF77) -g -o $@ $<
mpi_ringtopo: mpi_ringtopo.c Makefile
	$(MPICC) -g -o $@ $<
rank_test: rank_test.c Makefile
	$(MPICC) -g -o $@ $<
mpi_ringtopo_defunct: mpi_ringtopo_defunct.c Makefile
	$(MPICC) -g -o $@ $<
to_test: to_test.c Makefile
	$(MPICC) -g -o $@ $<
