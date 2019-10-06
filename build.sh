#!/bin/sh -e

TARGET=x86_64-elf-dogfood
TOOLCHAIN_FILE=/tmp/dogfood-toolchain.txt
OUTDIR=toolchain
MAKE_ARGS=-j8

# parse options
CLEAN=0
BINUTILS=0
GCC=0
NEWLIB=0
while [ "$1" != "" ]; do
	P="$1"
	case "$P" in
		-h)
			echo "usage: build.sh [-hc] [-bg] [-n]"
            echo ""
            echo " -h    this help"
            echo " -c    clean (forces a rebuild)"
            echo ""
            echo " -b    build: binutils"
            echo " -g    build: gcc"
            echo ""
            echo " -n    build: newlib"
			exit 1
			;;
        -c)
            CLEAN=1
			;;
        -b)
            BINUTILS=1
			;;
        -g)
            GCC=1
            ;;
        -n)
            NEWLIB=1
            ;;
        *)
            echo "unexpected parameter '$P'; use '-h' for help"
            exit 1
    esac
    shift
done

# clean
if [ "$CLEAN" -ne 0 ]; then
    rm -rf ${OUTDIR}
    mkdir -p ${OUTDIR}
fi
OUTDIR=`realpath ${OUTDIR}`

# dependencies
if [ ! -f ${OUTDIR}/bin/${TARGET}-ld ]; then
    echo "Note: enabling binutils build since it doesn't seem available"
    BINUTILS=1
fi
if [ ! -f ${OUTDIR}/bin/${TARGET}-gcc ]; then
    echo "Note: enabling gcc build since it doesn't seem available"
    GCC=1
fi
if [ ! -f ${OUTDIR}/usr/lib/libc.a ]; then
    echo "Note: enabling newlib build since it doesn't seem available"
    NEWLIB=1
fi

# binutils
if [ "$BINUTILS" -ne 0 ]; then
    echo "*** Building binutils"
    rm -rf build/binutils
    mkdir -p build/binutils
    cd build/binutils
    ../../userland/binutils-2.32/configure --target=${TARGET} --disable-nls --disable-werror --prefix=${OUTDIR}
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
fi

export PATH=${OUTDIR}/bin:${PATH}

# gcc
if [ "$GCC" -ne 0 ]; then
    echo "*** Building gcc"
    rm -rf build/gcc
    mkdir -p build/gcc
    cd build/gcc
    ../../userland/gcc-9.2.0/configure --target=${TARGET} --disable-nls --without-headers --enable-languages='c,c++' --prefix=${OUTDIR}
    make ${MAKE_ARGS} all-gcc
    make ${MAKE_ARGS} all-target-libgcc
    make ${MAKE_ARGS} install-gcc
    make ${MAKE_ARGS} install-target-libgcc
    cd ../..
fi

if [ ! -f ${TOOLCHAIN_FILE} ]; then
    echo "set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSROOT ${OUTDIR})
set(CMAKE_WARN_DEPRECATED OFF)
include(CMakeForceCompiler)
CMAKE_FORCE_C_COMPILER(x86_64-elf-dogfood-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(x86_64-elf-dogfood-g++ GNU)

set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})
" >> ${TOOLCHAIN_FILE}
fi

# Newlib
if [ "$NEWLIB" -ne 0 ]; then
    echo "*** Building newlib"
    rm -rf build/newlib
    mkdir build/newlib
    cd build/newlib
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../lib/newlib-3.1.0
    ninja install
    ln -sf ${OUTDIR}/include ${OUTDIR}/${TARGET}/include
    cd ../..
fi
