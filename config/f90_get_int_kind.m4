dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
dnl                         University Research and Technology
dnl                         Corporation.  All rights reserved.
dnl Copyright (c) 2004-2005 The University of Tennessee and The University
dnl                         of Tennessee Research Foundation.  All rights
dnl                         reserved.
dnl Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
dnl                         University of Stuttgart.  All rights reserved.
dnl Copyright (c) 2004-2005 The Regents of the University of California.
dnl                         All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

AC_DEFUN([OMPI_F90_GET_INT_KIND],[
# Determine kind of a FORTRAN integer.
# First arg is decimal range, 2nd arg is config var to define.

AC_MSG_CHECKING(kind for FORTRAN selected_int_kind($1))

cat > conftestf.f90 <<EOF
program main
    open(8, file="conftestval")
    write(8, fmt="(I5)") selected_int_kind($1)
    close(8)
end program
EOF

#
# Try the compilation and run.
#

OMPI_LOG_COMMAND([$FC $FCFLAGS $FCFLAGS_f90 -o conftest conftestf.f90 $LDFLAGS $LIBS],
	OMPI_LOG_COMMAND([./conftest], [HAPPY=1], [HAPPY=0]), [HAPPY=0])

ompi_ac_int_kind=-1
if test "$HAPPY" = "1" -a -f conftestval; then
    # get rid of leading spaces for eval assignment
    ompi_ac_int_kind=`sed 's/  *//' conftestval`
    AC_MSG_RESULT([$ompi_ac_int_kind])
    if test -n "$2"; then
	eval "$2=$ompi_ac_int_kind"
    fi
else
    AC_MSG_RESULT([unknown])

    OMPI_LOG_MSG([here is the fortran program:], 1)
    OMPI_LOG_FILE([conftestf.f90])

    AC_MSG_WARN([*** Problem running configure test!])
    AC_MSG_WARN([*** See config.log for details.])
    AC_MSG_ERROR([*** Cannot continue.])
fi
str="$2=$ompi_ac_int_kind"
eval $str

unset ompi_ac_int_kind HAPPY
/bin/rm -f conftest*])dnl
