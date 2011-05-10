dnl AC_PATH_MSRPC_MINGW([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for msrpc-mingw. Define MSRPC_CFLAGS and MSRPC_LIBS and MIDL_WRAPPER.
dnl
AC_DEFUN([AC_PATH_MSRPC_MINGW],
[dnl 
dnl Get the cflags and libraries from pkg-config
dnl

  pkg_config_args="msrpc-mingw-1.0"

  PKG_PROG_PKG_CONFIG([0.16])

  no_msrpc=""

  if test "x$PKG_CONFIG" = x ; then
    no_msrpc=yes
    PKG_CONFIG=no
  fi

  min_msrpc_mingw_version=ifelse([$1], ,0.1.0,$1)
  AC_MSG_CHECKING(for msrpc-mingw - version >= $min_msrpc_version)

  if test x$PKG_CONFIG != xno ; then
    if $PKG_CONFIG --uninstalled $pkg_config_args; then
      echo "Will use uninstalled version of msrpc-mingw found in PKG_CONFIG_PATH"
    fi

    if $PKG_CONFIG --atleast-version $min_msrpc_mingw_version $pkg_config_args; then
      :
    else
      no_msrpc=yes
    fi
  fi

  if test x"$no_msrpc" = x ; then
    MIDL_WRAPPER=`$PKG_CONFIG --variable=midl_wrapper msrpc-mingw-1.0`
    MSRPC_CFLAGS=`$PKG_CONFIG --cflags $pkg_config_args`
    MSRPC_LIBS=`$PKG_CONFIG --libs $pkg_config_args`
  fi

  if test "x$no_msrpc" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$PKG_CONFIG" = "no" ; then
       echo "*** A new enough version of pkg-config was not found."
       echo "*** See http://www.freedesktop.org/software/pkgconfig/"
     else
       echo "not found"
     fi
     MSRPC_CFLAGS=""
     MSRPC_LIBS=""
     MIDL_WRAPPER=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(MSRPC_CFLAGS)
  AC_SUBST(MSRPC_LIBS)
  AC_SUBST(MIDL_WRAPPER)
])
