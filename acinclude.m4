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


#
# Open MPI-specific tests
#

m4_include(config/c_get_alignment.m4)
m4_include(config/c_weak_symbols.m4)

m4_include(config/cxx_find_template_parameters.m4)
m4_include(config/cxx_find_template_repository.m4)
m4_include(config/cxx_have_exceptions.m4)
m4_include(config/cxx_find_exception_flags.m4)

m4_include(config/f77_check.m4)
m4_include(config/f77_check_type.m4)
m4_include(config/f77_find_ext_symbol_convention.m4)
m4_include(config/f77_get_alignment.m4)
m4_include(config/f77_get_fortran_handle_max.m4)
m4_include(config/f77_get_sizeof.m4)
m4_include(config/f77_get_value_true.m4)
m4_include(config/f77_check_logical_array.m4)
m4_include(config/f77_purge_unsupported_kind.m4)

m4_include(config/f90_check.m4)
m4_include(config/f90_check_type.m4)
m4_include(config/f90_find_module_include_flag.m4)
m4_include(config/f90_get_alignment.m4)
m4_include(config/f90_get_precision.m4)
m4_include(config/f90_get_range.m4)
m4_include(config/f90_get_sizeof.m4)
m4_include(config/f90_get_int_kind.m4)

m4_include(config/ompi_objc.m4)

m4_include(config/ompi_try_assemble.m4)
m4_include(config/ompi_config_asm.m4)

m4_include(config/ompi_case_sensitive_fs_setup.m4)
m4_include(config/ompi_check_broken_qsort.m4)
m4_include(config/ompi_check_compiler_works.m4)
m4_include(config/ompi_check_func_lib.m4)
m4_include(config/ompi_check_optflags.m4)
m4_include(config/ompi_check_icc.m4)
m4_include(config/ompi_check_gm.m4)
m4_include(config/ompi_check_mx.m4)
m4_include(config/ompi_check_bproc.m4)
m4_include(config/ompi_check_mvapi.m4)
m4_include(config/ompi_check_openib.m4)
m4_include(config/ompi_check_udapl.m4)
m4_include(config/ompi_check_package.m4)
m4_include(config/ompi_check_slurm.m4)
m4_include(config/ompi_check_tm.m4)
m4_include(config/ompi_check_xgrid.m4)
m4_include(config/ompi_check_vendor.m4)
m4_include(config/ompi_config_subdir.m4)
m4_include(config/ompi_config_subdir_args.m4)
m4_include(config/ompi_configure_options.m4)
m4_include(config/ompi_find_type.m4)
m4_include(config/ompi_functions.m4)
m4_include(config/ompi_get_version.m4)
m4_include(config/ompi_get_libtool_linker_flags.m4)
m4_include(config/ompi_load_platform.m4)
m4_include(config/ompi_mca.m4)
m4_include(config/ompi_setup_cc.m4)
m4_include(config/ompi_setup_cxx.m4)
m4_include(config/ompi_setup_f77.m4)
m4_include(config/ompi_setup_f90.m4)
m4_include(config/ompi_setup_libevent.m4)
m4_include(config/ompi_setup_wrappers.m4)
m4_include(config/ompi_make_stripped_flags.m4)
m4_include(config/ompi_install_dirs.m4)
m4_include(config/ompi_save_version.m4)

m4_include(config/ompi_check_pthread_pids.m4)
m4_include(config/ompi_config_pthreads.m4)
m4_include(config/ompi_config_solaris_threads.m4)
m4_include(config/ompi_config_threads.m4)

#
# The config/mca_no_configure_components.m4 file is generated by
# autogen.sh
#
m4_include(config/mca_no_configure_components.m4)

#
# mca_m4_config_include.m4 is generated by autogen.sh.  It includes
# the list of all component configure.m4 macros.
#
m4_include(config/mca_m4_config_include.m4)
