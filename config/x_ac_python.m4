AC_DEFUN([X_AC_PYTHON], [
  if test -z "$PYTHON"; then
    AC_MSG_ERROR([Failed to find python in your path])
  fi    
  STATPYTHON=$PYTHON
  AC_MSG_CHECKING([Python include dir])
  if test -z "$PYTHONINCLUDE"; then
    python_header_path=`$PYTHON -c "import distutils.sysconfig; \
      print(distutils.sysconfig.get_python_inc());"`
    AC_MSG_RESULT($python_header_path)
    PYTHONINCLUDE=$python_header_path
    CXXFLAGS="$CXXFLAGS -I$PYTHONINCLUDE/"
  fi
])
