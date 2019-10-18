#!/bin/sh -e

TARGET=x86_64-elf-dogfood
TOOLCHAIN_FILE=/tmp/dogfood-toolchain.txt
TOOLCHAIN=toolchain # where toolchain items get written
OUTDIR=target # cross-compiled binaries end up here
MAKE_ARGS=-j8

# parse options
CLEAN=0
BINUTILS=0
GCC=0
NEWLIB=0
DASH=0
COREUTILS=0
INIT=0
while [ "$1" != "" ]; do
	P="$1"
	case "$P" in
		-h)
			echo "usage: build.sh [-hc] [-bg] [-ndui]"
            echo ""
            echo " -h    this help"
            echo " -c    clean (forces a rebuild of toolchain)"
            echo " -a    build everything"
            echo ""
            echo " -b    build: binutils"
            echo " -g    build: gcc"
            echo ""
            echo " -n    build: newlib"
            echo " -d    build: dash"
            echo " -u    build: coreutils"
            echo " -i    build: init"
			exit 1
			;;
        -a)
            BINUTILS=1
            GCC=1
            NEWLIB=1
            DASH=1
            COREUTILS=1
            INIT=1
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
        -d)
            DASH=1
            ;;
        -u)
            COREUTILS=1
            ;;
        -i)
            INIT=1
            ;;
        *)
            echo "unexpected parameter '$P'; use '-h' for help"
            exit 1
    esac
    shift
done

# clean
if [ "$CLEAN" -ne 0 ]; then
    rm -rf ${TOOLCHAIN}
    rm -rf ${OUTDIR}
fi
mkdir -p ${TOOLCHAIN}
mkdir -p ${OUTDIR}
TOOLCHAIN=`realpath ${TOOLCHAIN}`
OUTDIR=`realpath ${OUTDIR}`

# dependencies
if [ ! -f ${TOOLCHAIN}/bin/${TARGET}-ld ]; then
    echo "Note: enabling binutils build since it doesn't seem available"
    BINUTILS=1
fi
if [ ! -f ${TOOLCHAIN}/bin/${TARGET}-gcc ]; then
    echo "Note: enabling gcc build since it doesn't seem available"
    GCC=1
fi
if [ ! -f ${TOOLCHAIN}/usr/lib/libc.a ]; then
    echo "Note: enabling newlib build since it doesn't seem available"
    NEWLIB=1
fi

# binutils
if [ "$BINUTILS" -ne 0 ]; then
    echo "*** Building binutils"
    rm -rf build/binutils
    mkdir -p build/binutils
    cd build/binutils
    ../../userland/binutils-2.32/configure --target=${TARGET} --disable-nls --disable-werror --prefix=${TOOLCHAIN}
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
fi

export PATH=${TOOLCHAIN}/bin:${PATH}

# gcc
if [ "$GCC" -ne 0 ]; then
    echo "*** Building gcc"
    rm -rf build/gcc
    mkdir -p build/gcc
    cd build/gcc
    ../../userland/gcc-9.2.0/configure --target=${TARGET} --disable-nls --without-headers --enable-languages='c,c++' --prefix=${TOOLCHAIN}
    make ${MAKE_ARGS} all-gcc
    make ${MAKE_ARGS} all-target-libgcc
    make ${MAKE_ARGS} install-gcc
    make ${MAKE_ARGS} install-target-libgcc
    cd ../..
fi

if [ ! -f ${TOOLCHAIN_FILE} ]; then
    echo "set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSROOT ${TOOLCHAIN})
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
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${TOOLCHAIN} ../../lib/newlib-3.1.0
    ninja install
    ln -sf ${TOOLCHAIN}/usr/include ${TOOLCHAIN}/${TARGET}/include
    cd ../..
fi

# Dash
if [ "$DASH" -ne 0 ]; then
    echo "*** Bulding dash"
    cd userland/dash-0.5.10.2
    make clean || true
    CFLAGS="--sysroot ${TOOLCHAIN} -DJOBS=0" ./configure --host=${TARGET} --prefix=${OUTDIR}
    make ${MAKE_ARGS}
    make install
    mv ${OUTDIR}/bin/dash ${OUTDIR}/bin/sh
    cd ../..
fi

# coreutils
if [ "$COREUTILS" -ne 0 ]; then
    echo "*** Building coreutils"
    cd userland/coreutils-8.31
    make clean || true
    CFLAGS="--sysroot ${TOOLCHAIN} -DOK_TO_USE_1S_CLOCK" ./configure --host=${TARGET} --prefix=${OUTDIR}
    make ${MAKE_ARGS}
    make install
    cd ../..
fi

# init
if [ "$INIT" -ne 0 ]; then
    echo "*** Building init"
    rm -rf build/init
    mkdir -p build/init
    cd build/init
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../userland/init
    ninja install
    cd ../..
fi


