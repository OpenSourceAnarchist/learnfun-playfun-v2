# playfun - Modern C++ NES AI Framework
# configure.ac - Build configuration with flexible optimization options
#
# Usage:
#   ./configure                              # Standard release (-O2)
#   ./configure --enable-debug               # Debug with sanitizers
#   ./configure --enable-performance         # Maximum performance
#   ./configure --with-cxx-standard=17       # Use C++17 instead of C++23
#   ./configure --with-opt-level=3           # Use -O3 instead of -Ofast

AC_PREREQ([2.69])
AC_INIT([playfun], [2.0.0], [https://github.com/gerow/playfun])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])

# ==============================================================================
# Compiler Selection (optional convenience flag)
# ==============================================================================
AC_ARG_WITH([compiler],
    [AS_HELP_STRING([--with-compiler=NAME],
        [Select compiler: gcc, clang, icx (Intel), or auto (default: auto)])],
    [selected_compiler=$withval],
    [selected_compiler=auto])

# Set CC/CXX based on --with-compiler selection
AS_CASE([$selected_compiler],
    [gcc],   [CC=gcc; CXX=g++; AC_MSG_NOTICE([Using GCC compiler suite])],
    [clang], [CC=clang; CXX=clang++; AC_MSG_NOTICE([Using Clang compiler suite])],
    [icx],   [CC=icx; CXX=icpx; AC_MSG_NOTICE([Using Intel oneAPI compiler suite])],
    [auto],  [AC_MSG_NOTICE([Auto-detecting compiler])],
    [AC_MSG_ERROR([Unknown compiler: $selected_compiler. Use gcc, clang, icx, or auto.])])

# Export for subprocesses if explicitly set
AS_IF([test "x$selected_compiler" != "xauto"],
    [export CC CXX])

# ==============================================================================
# Compiler Detection & Setup
# ==============================================================================
AC_PROG_CXX
AC_PROG_CC
AM_PROG_AR
LT_INIT
AM_PROG_CC_C_O

# Detect compiler family for flag tuning
AC_MSG_CHECKING([compiler family])
COMPILER_FAMILY="unknown"
case "$CXX" in
    *clang*) COMPILER_FAMILY="clang" ;;
    *icpx*)  COMPILER_FAMILY="intel" ;;
    *icpc*)  COMPILER_FAMILY="intel" ;;
    *g++*)   COMPILER_FAMILY="gcc" ;;
    *gcc*)   COMPILER_FAMILY="gcc" ;;
esac
AC_MSG_RESULT([$COMPILER_FAMILY])
AC_SUBST([COMPILER_FAMILY])

# ==============================================================================
# Configurable C++ Standard (default: C++23, configurable for iterative modernization)
# ==============================================================================
AC_ARG_WITH([cxx-standard],
    [AS_HELP_STRING([--with-cxx-standard=VER],
        [C++ standard version: 11, 14, 17, 20, or 23 (default: 23)])],
    [cxx_standard=$withval],
    [cxx_standard=23])

# Note: AX_CXX_COMPILE_STDCXX requires literal values at autoconf time,
# so we must use AS_CASE with explicit macro calls for each standard.
AS_CASE([$cxx_standard],
    [11], [AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])],
    [14], [AX_CXX_COMPILE_STDCXX([14], [noext], [mandatory])],
    [17], [AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])],
    [20], [AX_CXX_COMPILE_STDCXX([20], [noext], [mandatory])],
    [23], [AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])],
    [AC_MSG_ERROR([Invalid C++ standard: $cxx_standard. Use 11, 14, 17, 20, or 23.])])
AC_MSG_NOTICE([Requiring C++$cxx_standard support])

# ==============================================================================
# Build Mode Selection
# ==============================================================================
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
        [Enable debug mode with sanitizers and no optimization])],
    [enable_debug=$enableval],
    [enable_debug=no])

AC_ARG_ENABLE([performance],
    [AS_HELP_STRING([--enable-performance],
        [Enable maximum performance optimizations])],
    [enable_performance=$enableval],
    [enable_performance=no])

# Mutual exclusion check
if test "x$enable_debug" = "xyes" && test "x$enable_performance" = "xyes"; then
    AC_MSG_ERROR([Cannot enable both --enable-debug and --enable-performance])
fi

# ==============================================================================
# Performance Tuning Options (only relevant with --enable-performance)
# ==============================================================================
AC_ARG_WITH([opt-level],
    [AS_HELP_STRING([--with-opt-level=LEVEL],
        [Optimization level: 2, 3, or fast (default: 2 for release, fast for performance)])],
    [opt_level=$withval],
    [opt_level=default])

AC_ARG_WITH([lto],
    [AS_HELP_STRING([--with-lto=MODE],
        [Link-time optimization: no, yes, thin (Clang only), or full (default: no)])],
    [lto_mode=$withval],
    [lto_mode=no])

AC_ARG_ENABLE([native],
    [AS_HELP_STRING([--disable-native],
        [Disable -march=native for portable binaries])],
    [enable_native=$enableval],
    [enable_native=yes])

AC_ARG_WITH([align-functions],
    [AS_HELP_STRING([--with-align-functions=N],
        [Align functions to N bytes: no, 32, or 64 (default: 32 for performance)])],
    [align_functions=$withval],
    [align_functions=default])

# ==============================================================================
# Base Warning Flags
# ==============================================================================
BASE_WARN="-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
BASE_WARN="$BASE_WARN -Wno-register -Wno-deprecated -Wno-unused-result -Wno-write-strings"
BASE_CWARN="-fpermissive"

# ==============================================================================
# Flag Application Logic
# ==============================================================================

if test "x$enable_debug" = "xyes"; then
    # ========== DEBUG MODE ==========
    AC_MSG_NOTICE([Configuring DEBUG build])
    
    CFLAGS="-O0 -g3 $BASE_WARN $BASE_CWARN $CFLAGS"
    CXXFLAGS="-O0 -g3 $BASE_WARN $CXXFLAGS"
    
    # Sanitizers (check availability)
    AC_MSG_CHECKING([for AddressSanitizer support])
    saved_CXXFLAGS="$CXXFLAGS"
    CXXFLAGS="$CXXFLAGS -fsanitize=address"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
        [AC_MSG_RESULT([yes])
         CFLAGS="$CFLAGS -fsanitize=address"
         LDFLAGS="$LDFLAGS -fsanitize=address"],
        [AC_MSG_RESULT([no])])
    CXXFLAGS="$saved_CXXFLAGS"
    
    AC_MSG_CHECKING([for UndefinedBehaviorSanitizer support])
    saved_CXXFLAGS="$CXXFLAGS"
    CXXFLAGS="$CXXFLAGS -fsanitize=undefined"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
        [AC_MSG_RESULT([yes])
         CFLAGS="$CFLAGS -fsanitize=undefined"
         CXXFLAGS="$saved_CXXFLAGS -fsanitize=undefined"
         LDFLAGS="$LDFLAGS -fsanitize=undefined"],
        [AC_MSG_RESULT([no])
         CXXFLAGS="$saved_CXXFLAGS"])
    
    AC_DEFINE([DEBUG], [1], [Debug mode enabled])
    
elif test "x$enable_performance" = "xyes"; then
    # ========== PERFORMANCE MODE ==========
    AC_MSG_NOTICE([Configuring PERFORMANCE build])
    
    # Set defaults for performance mode
    if test "x$opt_level" = "xdefault"; then
        opt_level=fast
    fi
    if test "x$align_functions" = "xdefault"; then
        align_functions=32
    fi
    
    # Base optimization
    case "$opt_level" in
        2)    OPT_FLAGS="-O2" ;;
        3)    OPT_FLAGS="-O3" ;;
        fast) OPT_FLAGS="-Ofast" ;;
        *)    AC_MSG_ERROR([Invalid opt-level: $opt_level. Use 2, 3, or fast.]) ;;
    esac
    
    CFLAGS="$OPT_FLAGS $BASE_WARN $BASE_CWARN $CFLAGS"
    CXXFLAGS="$OPT_FLAGS $BASE_WARN $CXXFLAGS"
    
    # Native architecture
    if test "x$enable_native" = "xyes"; then
        AC_MSG_CHECKING([for -march=native support])
        saved_CXXFLAGS="$CXXFLAGS"
        CXXFLAGS="$CXXFLAGS -march=native"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
            [AC_MSG_RESULT([yes])
             CFLAGS="$CFLAGS -march=native"],
            [AC_MSG_RESULT([no])
             CXXFLAGS="$saved_CXXFLAGS"])
    fi
    
    # Function alignment
    if test "x$align_functions" != "xno"; then
        AC_MSG_CHECKING([for -falign-functions=$align_functions support])
        saved_CXXFLAGS="$CXXFLAGS"
        CXXFLAGS="$CXXFLAGS -falign-functions=$align_functions"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
            [AC_MSG_RESULT([yes])
             CFLAGS="$CFLAGS -falign-functions=$align_functions"],
            [AC_MSG_RESULT([no])
             CXXFLAGS="$saved_CXXFLAGS"])
    fi
    
    # LTO
    case "$lto_mode" in
        no)   ;;
        yes|full)
            AC_MSG_CHECKING([for LTO support])
            saved_CXXFLAGS="$CXXFLAGS"
            saved_LDFLAGS="$LDFLAGS"
            CXXFLAGS="$CXXFLAGS -flto"
            LDFLAGS="$LDFLAGS -flto"
            AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
                [AC_MSG_RESULT([yes])
                 CFLAGS="$CFLAGS -flto"],
                [AC_MSG_RESULT([no])
                 CXXFLAGS="$saved_CXXFLAGS"
                 LDFLAGS="$saved_LDFLAGS"])
            ;;
        thin)
            AC_MSG_CHECKING([for ThinLTO support])
            saved_CXXFLAGS="$CXXFLAGS"
            saved_LDFLAGS="$LDFLAGS"
            CXXFLAGS="$CXXFLAGS -flto=thin"
            LDFLAGS="$LDFLAGS -flto=thin"
            AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
                [AC_MSG_RESULT([yes])
                 CFLAGS="$CFLAGS -flto=thin"],
                [AC_MSG_RESULT([no])
                 CXXFLAGS="$saved_CXXFLAGS"
                 LDFLAGS="$saved_LDFLAGS"])
            ;;
        *)
            AC_MSG_ERROR([Invalid lto mode: $lto_mode. Use no, yes, thin, or full.])
            ;;
    esac
    
    # Compiler-specific optimizations
    if test "x$COMPILER_FAMILY" = "xgcc"; then
        # GCC-specific flags
        for flag in -fivopts -fmodulo-sched -ftree-vectorize; do
            AC_MSG_CHECKING([for $flag support])
            saved_CXXFLAGS="$CXXFLAGS"
            CXXFLAGS="$CXXFLAGS $flag"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
                [AC_MSG_RESULT([yes])
                 CFLAGS="$CFLAGS $flag"],
                [AC_MSG_RESULT([no])
                 CXXFLAGS="$saved_CXXFLAGS"])
        done
        # Graphite optimizations (requires isl)
        for flag in -fgraphite-identity -floop-nest-optimize; do
            AC_MSG_CHECKING([for $flag support (Graphite)])
            saved_CXXFLAGS="$CXXFLAGS"
            CXXFLAGS="$CXXFLAGS $flag"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
                [AC_MSG_RESULT([yes])
                 CFLAGS="$CFLAGS $flag"],
                [AC_MSG_RESULT([no])
                 CXXFLAGS="$saved_CXXFLAGS"])
        done
    elif test "x$COMPILER_FAMILY" = "xclang"; then
        # Clang-specific flags
        for flag in -fvectorize -fslp-vectorize; do
            AC_MSG_CHECKING([for $flag support])
            saved_CXXFLAGS="$CXXFLAGS"
            CXXFLAGS="$CXXFLAGS $flag"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
                [AC_MSG_RESULT([yes])
                 CFLAGS="$CFLAGS $flag"],
                [AC_MSG_RESULT([no])
                 CXXFLAGS="$saved_CXXFLAGS"])
        done
        # Polly (if available)
        AC_MSG_CHECKING([for Polly optimizer support])
        saved_CXXFLAGS="$CXXFLAGS"
        CXXFLAGS="$CXXFLAGS -mllvm -polly"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
            [AC_MSG_RESULT([yes])
             CFLAGS="$CFLAGS -mllvm -polly"
             CXXFLAGS="$CXXFLAGS -mllvm -polly-vectorizer=stripmine"],
            [AC_MSG_RESULT([no])
             CXXFLAGS="$saved_CXXFLAGS"])
    fi
    
    AC_DEFINE([NDEBUG], [1], [Release mode - disable assertions])
    
else
    # ========== STANDARD RELEASE ==========
    AC_MSG_NOTICE([Configuring STANDARD RELEASE build])
    
    if test "x$opt_level" = "xdefault"; then
        opt_level=2
    fi
    
    case "$opt_level" in
        2)    CFLAGS="-O2 $BASE_WARN $BASE_CWARN $CFLAGS"
              CXXFLAGS="-O2 $BASE_WARN $CXXFLAGS" ;;
        3)    CFLAGS="-O3 $BASE_WARN $BASE_CWARN $CFLAGS"
              CXXFLAGS="-O3 $BASE_WARN $CXXFLAGS" ;;
        fast) CFLAGS="-Ofast $BASE_WARN $BASE_CWARN $CFLAGS"
              CXXFLAGS="-Ofast $BASE_WARN $CXXFLAGS" ;;
        *)    AC_MSG_ERROR([Invalid opt-level: $opt_level]) ;;
    esac
fi

# ==============================================================================
# Dependencies
# ==============================================================================
PKG_CHECK_MODULES([SDL], [sdl])
PKG_CHECK_MODULES([SDL_NET], [SDL_net])
PKG_CHECK_MODULES([ZLIB], [zlib])
PKG_CHECK_MODULES([LIBPNG], [libpng])
PKG_CHECK_MODULES([PROTOBUF], [protobuf])

AC_CHECK_PROG([PROTOC], [protoc], [protoc])
AS_IF([test "x${PROTOC}" = "x"],
    [AC_MSG_ERROR([ProtoBuf compiler "protoc" not found.])])

# ==============================================================================
# Output
# ==============================================================================
AC_CONFIG_FILES([Makefile
                 cc-lib/Makefile
                 tasbot/Makefile])
AC_OUTPUT

# Print configuration summary
echo ""
echo "============================================================"
echo " playfun Build Configuration"
echo "============================================================"
echo " C++ Standard       : C++$cxx_standard"
echo " Compiler           : $CXX ($COMPILER_FAMILY)"
echo " Debug Mode         : $enable_debug"
echo " Performance Mode   : $enable_performance"
if test "x$enable_performance" = "xyes"; then
echo " Optimization Level : -O$opt_level"
echo " LTO Mode           : $lto_mode"
echo " Native Arch        : $enable_native"
echo " Function Align     : $align_functions"
fi
echo ""
echo " CXXFLAGS: $CXXFLAGS"
echo " LDFLAGS:  $LDFLAGS"
echo "============================================================"
echo ""
