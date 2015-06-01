AC_DEFUN([X_AC_MPICC], [ 
  AC_ARG_VAR(MPICC, [MPI C compiler wrapper])
  AC_ARG_VAR(MPICXX, [MPI C++ compiler wrapper])
  AC_ARG_VAR(MPIF77, [MPI Fortran compiler wrapper])
  AC_CHECK_PROGS(MPICC, mpigcc mpicc mpiicc mpxlc mpixlc, $CC)
  AC_CHECK_PROGS(MPICXX, mpig++ mpiCC mpicxx mpiicpc mpxlC mpixlC, $CXX)
  AC_CHECK_PROGS(MPIF77, mpigfortran mpif77 mpiifort mpxlf mpixlf, $F77)
  AC_SUBST(MPICC)
  AC_SUBST(MPICXX)
  AC_SUBST(MPIF77)
])

