AC_DEFUN([X_AC_CUDAGDB], [
  AM_CONDITIONAL([ENABLE_CUDAGDB], false)
  AC_ARG_ENABLE(cudagdb,
    [AS_HELP_STRING([--enable-cudagdb],[Enable to include CUDA GDB BE prototype])],
    [ENABLE_CUDAGDB="yes"
      AM_CONDITIONAL([ENABLE_CUDAGDB], true)
    ]
  )

  AC_ARG_WITH(cudagdb,
    [AS_HELP_STRING([--with-cudagdb=path],
      [Use cmd as the default remote shell]
    )],
    [withcudagdb="$withval"
      AM_CONDITIONAL([ENABLE_CUDAGDB], true)
    ],
    [withcudagdb=cuda-gdb]
  )
  AC_PATH_PROG(cudagdbcmd,$withcudagdb,[cuda-gdb],/,/usr/local/bin:/usr/bin:/bin/usr/local/cuda/bin)

])
