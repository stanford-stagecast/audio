#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([chazonance], [0.1], [keithw@cs.stanford.edu])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

# Add picky CXXFLAGS
CXX17_FLAGS="-std=c++17 -pthread"
PICKY_CXXFLAGS="-Wpedantic -Wall -Wextra -Weffc++ -Werror"
AC_SUBST([CXX17_FLAGS])
AC_SUBST([OPT_CXXFLAGS])
AC_SUBST([PICKY_CXXFLAGS])

# Change default CXXflags
: ${CXXFLAGS="-g -Ofast -march=native -mtune=native"}

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for libraries.
PKG_CHECK_MODULES([fftw3f],[fftw3f])
PKG_CHECK_MODULES([alsa],[alsa])
PKG_CHECK_MODULES([sndfile],[sndfile])

# Checks for header files.
AC_LANG_PUSH(C++)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CXX17_FLAGS"
AC_CHECK_HEADERS([boost/align/aligned_allocator.hpp], [], [AC_MSG_ERROR([Missing boost headers (need to install libboost-dev?)])])
CPPFLAGS="$save_CPPFLAGS"
AC_LANG_POP(C++)

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([
    Makefile
])
AC_OUTPUT
