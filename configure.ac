#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([RadiiU],
        [0.1.3],
        [https://github.com/dkosmari/RadiiU/issues],
        [radiiu],
        [https://github.com/dkosmari/RadiiU])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AC_CANONICAL_HOST

DEVKITPRO_WUT_OPT_INIT

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_PROG_RANLIB

AC_PROG_CXX
AX_APPEND_COMPILE_FLAGS([-std=c++23], [CXX])
AC_LANG([C++])

AS_VAR_IF([enable_wiiu], [yes],
          [
              AC_MSG_NOTICE([building Wii U app])
              DEVKITPRO_PPC_SETUP
              DEVKITPRO_WUT_SETUP
          ])
AM_CONDITIONAL([ENABLE_WIIU], [test x$enable_wiiu = xyes])


PKG_CHECK_MODULES_STATIC([CURL], [libcurl])
PKG_CHECK_MODULES([JANSSON], [jansson])
PKG_CHECK_MODULES([MPG123], [libmpg123])
PKG_CHECK_MODULES([SDL], [sdl2 SDL2_image])
PKG_CHECK_MODULES([FT], [freetype2])

AC_MSG_NOTICE([PATH is $PATH])
AC_MSG_NOTICE([CPPFLAGS is "$CPPFLAGS"])
AC_MSG_NOTICE([CFLAGS is "$CFLAGS"])
AC_MSG_NOTICE([CXXFLAGS is "$CXXFLAGS"])
AC_MSG_NOTICE([LDFLAGS is "$LDFLAGS"])
AC_MSG_NOTICE([LIBS is "$LIBS"])


AX_SUBDIRS_CONFIGURE([external/curlxx],
                     [[--enable-static],
                      [CPPFLAGS=$CPPFLAGS],
                      [CFLAGS=$CFLAGS],
                      [CXXFLAGS=$CXXFLAGS],
                      [LDFLAGS=$LDFLAGS],
                      [LIBS=$LIBS],
                      [PATH=$PATH]])

AX_SUBDIRS_CONFIGURE([external/mpg123xx],
                     [[--disable-examples],
                      [CPPFLAGS=$CPPFLAGS],
                      [CFLAGS=$CFLAGS],
                      [CXXFLAGS=$CXXFLAGS],
                      [LDFLAGS=$LDFLAGS],
                      [LIBS=$LIBS],
                      [PATH=$PATH]])

AX_SUBDIRS_CONFIGURE([external/sdl2xx],
                     [[--enable-image],
                      [--disable-mixer],
                      [--disable-ttf],
                      [--disable-examples],
                      [CPPFLAGS=$CPPFLAGS],
                      [CFLAGS=$CFLAGS],
                      [CXXFLAGS=$CXXFLAGS],
                      [LDFLAGS=$LDFLAGS],
                      [LIBS=$LIBS],
                      [PATH=$PATH]])


AC_CONFIG_FILES([Makefile
                 external/Makefile])
AC_OUTPUT
