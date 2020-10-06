#                                               -*- Autoconf -*-
# Process this file with `autogen.sh` to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mush], [0.1], [keithw@cs.stanford.edu])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

# Add picky CXXFLAGS
CXX17_FLAGS="-std=c++17 -pthread"
PICKY_CXXFLAGS="-Wpedantic -Wall -Wextra -Weffc++ -Werror"
AC_SUBST([CXX17_FLAGS])
AC_SUBST([PICKY_CXXFLAGS])

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB
AC_PATH_PROG([CLANG_FORMAT], [clang-format], [])
AS_IF([test x"$CLANG_FORMAT" = x],
  [AC_MSG_ERROR([cannot find clang-format])])

# Checks for header files.
AC_LANG_PUSH(C++)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CXX17_FLAGS"

CPPFLAGS="$save_CPPFLAGS"
AC_LANG_POP(C++)

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    src/util/Makefile
])
AC_OUTPUT
