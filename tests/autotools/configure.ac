AC_INIT([basic-test],
        [1.0])

AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 -Wall foreign subdir-objects tar-pax no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O

AC_PATH_MSRPC_MINGW([0.1.0], :,
  AC_MSG_ERROR([
*** msrpc-mingw 0.1.0 or better is required.]))

AC_PATH_PROG([MIDL], [midl], no)
if test "$MIDL" = "no"; then
  AC_MSG_ERROR([
*** Could not find the Microsoft interface compiler MIDL.EXE.
*** This program is available as part of the MS Windows SDK, in
*** the Win32 Development Tools package.])
fi

AC_SUBST(MIDL)

AC_CONFIG_FILES([
	Makefile
])

AC_OUTPUT
