#!/bin/sh -e

TARGET=x86_64-elf-dogfood
TOOLCHAIN_FILE=/tmp/dogfood-toolchain.txt
TOOLCHAIN=toolchain # where toolchain items get written
OUTDIR=target # cross-compiled binaries end up here
MAKE_ARGS=-j8

# parse options
CLEAN=0
TOOLCHAIN_HOST=0
LIBS=0
DASH=0
COREUTILS=0
INIT=0
TOOLCHAIN_TARGET=0
while [ "$1" != "" ]; do
	P="$1"
	case "$P" in
		-h)
			echo "usage: build.sh [-hca] [-t] [-l] [-duiT]"
            echo ""
            echo " -h    this help"
            echo " -c    clean (forces a rebuild of toolchain)"
            echo " -a    build everything"
            echo ""
            echo " -t    build: binutils/gcc/libstdc++ for host"
            echo ""
            echo " -l    build: libraries for host"
            echo ""
            echo " -d    build: dash"
            echo " -u    build: coreutils"
            echo " -i    build: init"
            echo ""
            echo " -T    build: binutils+gcc for target"
			exit 1
			;;
        -a)
            TOOLCHAIN_HOST=1
            LIBS=1
            DASH=1
            COREUTILS=1
            INIT=1
            TOOLCHAIN_TARGET=1
            ;;
        -c)
            CLEAN=1
			;;
        -t)
            TOOLCHAIN_HOST=1
            ;;
        -l)
            LIBS=1
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
        -T)
            TOOLCHAIN_TARGET=1
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

SYSROOT=/tmp/dogfood-sysroot
mkdir -p ${SYSROOT}/usr/include

if [ ! -f "${TOOLCHAIN}/bin/${TARGET}-ld" ]; then
    echo "*** Building binutils (toolchain)"
    rm -rf build/binutils
    mkdir -p build/binutils
    cd build/binutils
    ../../userland/binutils-2.32/configure --target=${TARGET} --disable-nls --disable-werror --prefix=${TOOLCHAIN} --with-sysroot=${SYSROOT}
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
fi

export PATH=${TOOLCHAIN}/bin:${PATH}

# gcc
if [ ! -f "${TOOLCHAIN}/bin/${TARGET}-gcc" ]; then
    echo "*** Building gcc (toolchain)"
    rm -rf build/gcc
    mkdir -p build/gcc
    cd build/gcc
    ../../userland/gcc-9.2.0/configure --target=${TARGET} --disable-nls --without-headers --enable-languages='c,c++' --prefix=${TOOLCHAIN} --disable-libstdcxx --disable-build-with-cxx --disable-libssp --disable-libquadmath --with-sysroot=${SYSROOT} --with-gxx-include-dir=${SYSROOT}/usr/include/c++/9.2.0
    # we can't build everything yet as libgcc requires header files from libc;
    # settle for just gcc now
    make ${MAKE_ARGS} all-gcc
    make ${MAKE_ARGS} install-gcc
    cd ../..
fi

if [ ! -f ${TOOLCHAIN_FILE} ]; then
    echo "set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSROOT ${SYSROOT})
set(CMAKE_WARN_DEPRECATED OFF)
include(CMakeForceCompiler)
CMAKE_FORCE_C_COMPILER(x86_64-elf-dogfood-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(x86_64-elf-dogfood-g++ GNU)

set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})
" >> ${TOOLCHAIN_FILE}
fi

if [ ! -f "${SYSROOT}/usr/include/dogfood/types.h" ]; then
    echo "*** Installing kernel headers (sysroot)"
    rm -rf build/headers-target
    mkdir build/headers-target
    cd build/headers-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${SYSROOT} ../../kernel-headers
    ninja install
    cd ../..
fi


if [ ! -f "${SYSROOT}/usr/lib/libc.a" ]; then
    echo "*** Building newlib (sysroot)"
    rm -rf build/newlib
    mkdir build/newlib
    cd build/newlib
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${SYSROOT} ../../lib/newlib-3.1.0
    ninja install
    cd ../..
fi

if [ ! -f "${TOOLCHAIN}/lib/gcc/${TARGET}/9.2.0/libgcc.a" ]; then
    echo "*** Building libgcc (sysroot)"
    cd build/gcc
    make ${MAKE_ARGS} all-target-libgcc
    make ${MAKE_ARGS} install-target-libgcc
    cd ../..
fi

#if [ ! -d ${OUTDIR}/${TARGET}/include ]; then
#    ln -sf ${TOOLCHAIN}/usr/include ${TOOLCHAIN}/${TARGET}/include
#fi

# libstdc++
if [ ! -f "${SYSROOT}/usr/lib/libstdc++.a" ]; then
    echo "*** Building libstdcxx (toolchain)"
    rm -rf build/libstdcxx
    mkdir -p build/libstdcxx
    cd build/libstdcxx
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/gcc-9.2.0/libstdc++-v3/configure --host=${TARGET} --target=${TARGET} --prefix=${SYSROOT}/usr
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
fi

# Dash
if [ ! -f "${OUTDIR}/bin/sh" ]; then
    echo "*** Building dash (target)"
    cd userland/dash-0.5.10.2
    make clean || true
    CFLAGS="--sysroot ${SYSROOT} -DJOBS=0" ./configure --host=${TARGET} --prefix=${OUTDIR}
    make ${MAKE_ARGS}
    make install
    mv ${OUTDIR}/bin/dash ${OUTDIR}/bin/sh
    cd ../..
fi

# coreutils
if [ ! -f "${OUTDIR}/bin/ls" ]; then
    echo "*** Building coreutils (target)"
    cd userland/coreutils-8.31
    make clean || true
    CFLAGS="--sysroot ${SYSROOT} -DOK_TO_USE_1S_CLOCK" ./configure --host=${TARGET} --prefix=${OUTDIR}
    make ${MAKE_ARGS}
    make install
    cd ../..
fi

# init
if [ ! -f "${OUTDIR}/sbin/init" ]; then
    echo "*** Building init (target)"
    rm -rf build/init
    mkdir -p build/init
    cd build/init
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../userland/init
    ninja install
    cd ../..
fi

if [ ! -f "${OUTDIR}/bin/ld" ]; then
    echo "*** Building binutils (target)"
    rm -rf build/binutils-target
    mkdir -p build/binutils-target
    cd build/binutils-target
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/binutils-2.32/configure --host=${TARGET} --target=${TARGET} --disable-nls --disable-werror --prefix=/usr
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install DESTDIR=${OUTDIR}
    cd ../..
fi

if [ ! -f "${OUTDIR}/bin/gcc" ]; then
    echo "*** Building gcc (target)"
    rm -rf build/gcc-target
    mkdir -p build/gcc-target
    cd build/gcc-target
    CFLAGS="--sysroot ${SYSROOT}" CXXFLAGS="--sysroot ${SYSROOT}" ../../userland/gcc-9.2.0/configure --host=${TARGET} --target=${TARGET} --disable-nls --disable-werror --enable-languages='c,c++' --prefix=/usr
    #--with-gxx-include-dir=${OUTDIR}/usr/include/c++/9.2.0
    # XXX for some reason, we need to explicitely configure/build the host
    # libcpp; otherwise it inherits our sysroot which messes things up...
    make ${MAKE_ARGS} configure-build-libcpp
    (cd build-x86_64-pc-linux-gnu/libcpp && make ${MAKE_ARGS})
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install DESTDIR=${OUTDIR}
    cd ../..
fi

if [ ! -f "${TARGET}/lib/libc.a" ]; then
    echo "*** Building newlib (target)"
    rm -rf build/newlib-target
    mkdir build/newlib-target
    cd build/newlib-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../lib/newlib-3.1.0
    ninja install
    #mv ${OUTDIR}/usr/include ${OUTDIR}/${TARGET}/include
    cd ../..
fi

if [ ! -f "${TARGET}/include/dogfood/types.h" ]; then
    echo "*** Installing kernel headers (target)"
    rm -rf build/headers-target
    mkdir build/headers-target
    cd build/headers-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../kernel-headers
    ninja install
    #mv ${OUTDIR}/usr/include/dogfood ${OUTDIR}/${TARGET}/include
    cd ../..
fi
