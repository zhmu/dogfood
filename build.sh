#!/bin/sh -e

. ./settings.sh

OUTDIR=target # cross-compiled binaries end up here
OUTDIR_KERNEL=target-kernel # kernel ends up here
OUTDIR_IMAGES=images # images end up here
MAKE_ARGS=

# parse options
CLEAN_TARGET=0
BUILD_SYSROOT=0
BUILD_KERNEL=0
BUILD_TARGET=0
ONLY_REQUESTED_TARGETS=0
while [ "$1" != "" ]; do
	P="$1"
	case "$P" in
		-h)
			echo "usage: build.sh [-hco] [-skt]"
            echo ""
            echo " -h    this help"
            echo " -c    clean target"
            echo " -o    only build requested targets"
            echo ""
            echo " -s    (re)build sysroot"
            echo " -k    (re)build kernel"
            echo " -t    (re)build target"
            exit 1
			;;
        -c)
            CLEAN_TARGET=1
            ;;
        -o)
            ONLY_REQUESTED_TARGETS=1
            ;;
        -s)
            BUILD_SYSROOT=1
            ;;
        -k)
            BUILD_KERNEL=1
            ;;
        -t)
            BUILD_TARGET=1
            ;;
        *)
            echo "unexpected parameter '$P'; use '-h' for help"
            exit 1
    esac
    shift
done

# try to auto-detect the number of cpus in this system
if [ -f /proc/cpuinfo ]; then
    NCPUS=`grep processor /proc/cpuinfo|wc -l`
    MAKE_ARGS="${MAKE_ARGS} -j${NCPUS}"
fi

# clean
if [ "$CLEAN_TARGET" -ne 0 ]; then
    rm -rf ${OUTDIR}
    rm -rf ${OUTDIR_KERNEL}
fi
mkdir -p ${TOOLCHAIN}
mkdir -p ${OUTDIR}
mkdir -p ${OUTDIR_KERNEL}
mkdir -p ${OUTDIR_IMAGES}
TOOLCHAIN=`realpath ${TOOLCHAIN}`
OUTDIR=`realpath ${OUTDIR}`
OUTDIR_KERNEL=`realpath ${OUTDIR_KERNEL}`

export PATH=${TOOLCHAIN}/bin:${PATH}

KERNEL_DIRTY=0
TARGET_DIRTY=0

if [ "$BUILD_SYSROOT" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${SYSROOT}/usr/include/dogfood/types.h" \) ]; then
    echo "*** Installing kernel headers (sysroot)"
    rm -rf build/headers-target
    mkdir build/headers-target
    cd build/headers-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${SYSROOT} ../../kernel-headers
    ninja install
    cd ../..
fi


if [ "$BUILD_SYSROOT" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${SYSROOT}/usr/lib/libc.a" \) ]; then
    echo "*** Building newlib (sysroot)"
    rm -rf build/newlib
    mkdir build/newlib
    cd build/newlib
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${SYSROOT} ../../lib/newlib-3.1.0
    ninja install
    cd ../..
fi

if [ "$BUILD_SYSROOT" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${TOOLCHAIN}/lib/gcc/${TARGET}/${GCC_VERSION}/libgcc.a" \) ]; then
    echo "*** Building libgcc (sysroot)"
    rm -rf build/gcc-libgcc
    mkdir -p build/gcc-libgcc
    cd build/gcc-libgcc
    ../../userland/gcc-${GCC_VERSION}/configure --target=${TARGET} --disable-nls --without-headers --enable-languages='c,c++' --prefix=${TOOLCHAIN} --disable-libstdcxx --disable-build-with-cxx --disable-libssp --disable-libquadmath --with-sysroot=${SYSROOT} --with-gxx-include-dir=${SYSROOT}/usr/include/c++/${GCC_VERSION}
    make ${MAKE_ARGS} all-target-libgcc
    make ${MAKE_ARGS} install-target-libgcc
    cd ../..
fi

if [ "$BUILD_SYSROOT" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${SYSROOT}/usr/lib/libstdc++.a" \) ]; then
    echo "*** Building libstdcxx (sysroot)"
    rm -rf build/libstdcxx
    mkdir -p build/libstdcxx
    cd build/libstdcxx
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/gcc-${GCC_VERSION}/libstdc++-v3/configure --host=${TARGET} --target=${TARGET} --prefix=${SYSROOT}/usr
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
fi

if [ "${BUILD_KERNEL}" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_KERNEL}/kernel.mb" \) ]; then
    echo "*** Building kernel"
    rm -rf build/kernel
    mkdir -p build/kernel
    cd build/kernel
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR_KERNEL} ../..
    ninja kernel_mb install
    cd ../..
    KERNEL_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/bin/sh" \) ]; then
    echo "*** Building dash (target)"
    cd userland/dash-0.5.10.2
    make clean || true
    CFLAGS="--sysroot ${SYSROOT} -DJOBS=0" ./configure --host=${TARGET} --prefix=${OUTDIR}
    make ${MAKE_ARGS}
    make install
    mv ${OUTDIR}/bin/dash ${OUTDIR}/bin/sh
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/bin/ls" \) ]; then
    echo "*** Building coreutils (target)"
    cd userland/coreutils-8.31
    make clean || true
    CFLAGS="--sysroot ${SYSROOT} -DOK_TO_USE_1S_CLOCK" ./configure --host=${TARGET} --prefix=${OUTDIR}
    make ${MAKE_ARGS}
    make install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/sbin/init" \) ]; then
    echo "*** Building init (target)"
    rm -rf build/init
    mkdir -p build/init
    cd build/init
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../userland/init
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/bin/ps" \) ]; then
    echo "*** Building ps (target)"
    rm -rf build/ps
    mkdir -p build/ps
    cd build/ps
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../userland/ps
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/usr/bin/strace" \) ]; then
    echo "*** Building strace (target)"
    rm -rf build/strace
    mkdir -p build/strace
    cd build/strace
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../userland/strace
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/usr/bin/ld" \) ]; then
    echo "*** Building binutils (target)"
    rm -rf build/binutils-target
    mkdir -p build/binutils-target
    cd build/binutils-target
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/binutils-${BINUTILS_VERSION}/configure --host=${TARGET} --target=${TARGET} --disable-nls --disable-werror --prefix=/usr
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install DESTDIR=${OUTDIR}
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${SYSROOT}/lib/libgmp.a" \) ]; then
    echo "*** Building gmp (sysroot)"
    rm -rf build/gmp-sysroot
    mkdir -p build/gmp-sysroot
    cd build/gmp-sysroot
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/gmp-${GMP_VERSION}/configure --host=${TARGET} --target=${TARGET} --prefix=${SYSROOT}
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${SYSROOT}/lib/libmpfr.a" \) ]; then
    echo "*** Building mpfr (sysroot)"
    rm -rf build/mpfr-sysroot
    mkdir -p build/mpfr-sysroot
    cd build/mpfr-sysroot
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/mpfr-${MPFR_VERSION}/configure --host=${TARGET} --target=${TARGET} --prefix=${SYSROOT} --with-gmp=${SYSROOT}
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${SYSROOT}/lib/libmpc.a" \) ]; then
    echo "*** Building mpc (target)"
    rm -rf build/mpc-sysroot
    mkdir -p build/mpc-sysroot
    cd build/mpc-sysroot
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/mpc-${MPC_VERSION}/configure --host=${TARGET} --target=${TARGET} --prefix=${SYSROOT} --with-gmp=${SYSROOT}
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/usr/bin/gcc" \) ]; then
    echo "*** Building gcc (target)"
    rm -rf build/gcc-target
    mkdir -p build/gcc-target
    cd build/gcc-target
    CFLAGS="--sysroot ${SYSROOT}" CXXFLAGS="--sysroot ${SYSROOT}" ../../userland/gcc-${GCC_VERSION}/configure --host=${TARGET} --target=${TARGET} --disable-nls --disable-werror --enable-languages='c,c++' --prefix=/usr --with-gmp=${SYSROOT}
    # XXX for some reason, we need to explicitely configure/build the host
    # libcpp; otherwise it inherits our sysroot which messes things up...
    make ${MAKE_ARGS} configure-build-libcpp
    (cd build-x86_64-pc-linux-gnu/libcpp && make ${MAKE_ARGS})
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install DESTDIR=${OUTDIR}
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/usr/lib/libc.a" \) ]; then
    echo "*** Building newlib (target)"
    rm -rf build/newlib-target
    mkdir build/newlib-target
    cd build/newlib-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../lib/newlib-3.1.0
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR}/usr/include/dogfood/types.h" \) ]; then
    echo "*** Installing kernel headers (target)"
    rm -rf build/headers-target
    mkdir build/headers-target
    cd build/headers-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR} ../../kernel-headers
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$TARGET_DIRTY" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_IMAGES}/ext2.img" \) ]; then
    echo "*** Creating ext2 image..."
    rm -f ${OUTDIR_IMAGES}/ext2.img
    mkfs.ext2 -d ${OUTDIR} ${OUTDIR_IMAGES}/ext2.img 1g
fi

if [ "$KERNEL_DIRTY" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_IMAGES}/kernel.iso" \) ]; then
    echo "*** Creating kernel ISO image..."
    mkdir -p build/kernel-iso
    mkdir -p build/kernel-iso/boot/grub
    cp build/kernel/kernel/kernel.mb build/kernel-iso
    echo "set timeout=0
set default=0

menuentry "Dogfood" {
    multiboot /kernel.mb
    boot
}
" > build/kernel-iso/boot/grub/grub.cfg
    grub-mkrescue -o ${OUTDIR_IMAGES}/kernel.iso build/kernel-iso
fi

if [ "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_IMAGES}/run.sh" ]; then
    echo "qemu-system-x86_64 -serial stdio -hda ext2.img -cdrom kernel.iso -boot d \$@" > ${OUTDIR_IMAGES}/run.sh
    chmod 755 ${OUTDIR_IMAGES}/run.sh
fi
