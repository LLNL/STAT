      program Hello_World

!************************************************************************
!*  
!*     Name: Hello_World
!*
!*     Purpose: Simple "hello world" MPI program.
!*
!*     Revision History:
!*
!*     10-Nov-04 -- SAF; initial development of Hello_World.
!*
!************************************************************************

      implicit none

      include "mpif.h"

      integer :: my_rank, num_procs, mpi_ierr, getpid, my_reslen, reslen
      integer :: rank_inx, mpi_status(MPI_STATUS_SIZE), my_pid, pid
      integer :: master_rank = 0
      character (len = MPI_MAX_PROCESSOR_NAME) :: my_hostname, hostname

      integer :: input

!     Begin Hello_World.

!     Initialize the MPI library and get initial process data.

      call MPI_Init (mpi_ierr)
      call MPI_Comm_rank (MPI_COMM_WORLD, my_rank,   mpi_ierr)
      call MPI_Comm_size (MPI_COMM_WORLD, num_procs, mpi_ierr)

!     Have the master write out the total number of MPI proceses.

      if (my_rank .eq. master_rank) then
         write (6, 610) MPI_VERSION, MPI_SUBVERSION
         write (6, 600) num_procs
      endif

!     Have each MPI processes get its processor name and pid.

      call MPI_Get_processor_name (my_hostname, my_reslen, mpi_ierr)
      my_pid = getpid()

!     The master gathers up all the process data and writes a 
!     "hello world" message.

      do rank_inx = 0, num_procs - 1
         if ((rank_inx .eq. my_rank) .and.
     &       (my_rank .eq. master_rank)) then

            write (6, 601) master_rank, my_hostname(1:my_reslen), 
     &                     my_pid

         else
            if (my_rank .eq. master_rank) then

               call MPI_Recv (reslen, 1, MPI_INTEGER, rank_inx, 
     &                        1000, MPI_COMM_WORLD, mpi_status, 
     &                        mpi_ierr)
               call MPI_Recv (pid, 1, MPI_INTEGER, rank_inx, 
     &                        1001, MPI_COMM_WORLD, mpi_status, 
     &                        mpi_ierr)
               call MPI_Recv (hostname, reslen, MPI_CHARacter, 
     &                        rank_inx, 1002, MPI_COMM_WORLD, 
     &                        mpi_status, mpi_ierr)
               write (6, 601) rank_inx, hostname(1:reslen), pid

            elseif (my_rank .eq. rank_inx .and. my_rank .ne. 2) then

               call MPI_Send (my_reslen, 1, MPI_INTEGER, 
     &                        master_rank, 1000, MPI_COMM_WORLD,
     &                        mpi_ierr)
               call MPI_Send (my_pid, 1, MPI_INTEGER, 
     &                        master_rank, 1001, MPI_COMM_WORLD,
     &                        mpi_ierr)
               call MPI_Send (my_hostname, my_reslen, MPI_CHARACTER, 
     &                        master_rank, 1002, MPI_COMM_WORLD,
     &                        mpi_ierr)

            endif
         endif
      enddo         

!     Close up the MPI library.
      call MPI_Barrier(MPI_COMM_WORLD, mpi_ierr)
      call MPI_Finalize (mpi_ierr)

 600  format ("Total Number of Processes = ", i6)
 601  format ("Hello World from rank = ", i6, ", host = ", a,
     &        ", pid = ", i12)
 610  format ("MPI Version: ", i1, ".", i1)

!     End Hello_World.

      stop
      end
