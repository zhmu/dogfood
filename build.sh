#!/bin/sh -e

. ./settings.sh

OUTDIR=target # cross-compiled binaries end up here
OUTDIR_EXT2=${OUTDIR}/ext2 # used to generate ext2 image
OUTDIR_EFI=${OUTDIR}/efi # used to generate efi image
OUTDIR_IMAGES=${OUTDIR}/images # images end up here
MAKE_ARGS=

# parse options
CLEAN_TARGET=0
BUILD_SYSROOT=0
BUILD_KERNEL=0
BUILD_LOADER=0
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
            echo " -l    (re)build loader"
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
        -l)
            BUILD_LOADER=1
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
fi
mkdir -p ${TOOLCHAIN}
mkdir -p ${OUTDIR_EXT2}
mkdir -p ${OUTDIR_EFI}
mkdir -p ${OUTDIR_IMAGES}
TOOLCHAIN=`realpath ${TOOLCHAIN}`
OUTDIR_EXT2=`realpath ${OUTDIR_EXT2}`
OUTDIR_EFI=`realpath ${OUTDIR_EFI}`

export PATH=${TOOLCHAIN}/bin:${PATH}

TARGET_DIRTY=0
LOADER_DIRTY=0
IMAGE_DIRTY=0

mkdir -p build

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

if [ "${BUILD_KERNEL}" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/kernel.elf" \) ]; then
    echo "*** Building kernel"
    rm -rf build/kernel
    mkdir -p build/kernel
    cd build/kernel
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} ../..
    ninja kernel.elf
    cd ../..
    cp build/kernel/kernel/kernel.elf ${OUTDIR_EXT2}
    TARGET_DIRTY=1
fi

if [ "${BUILD_LOADER}" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EFI}/efi/boot/bootx64.efi" \) ]; then
    echo "*** Building loader"
    rm -rf build/loader
    mkdir -p build/loader
    cd build/loader
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} ../../loader
    ninja
    mkdir -p ${OUTDIR_EFI}/efi/boot
    cp loader.efi ${OUTDIR_EFI}/efi/boot/bootx64.efi
    cd ../..
    LOADER_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/bin/sh" \) ]; then
    echo "*** Building dash (target)"
    cd userland/dash-0.5.10.2
    make clean || true
    CFLAGS="--sysroot ${SYSROOT} -DJOBS=0" ./configure --host=${TARGET} --prefix=${OUTDIR_EXT2}
    make ${MAKE_ARGS}
    make install
    mv ${OUTDIR_EXT2}/bin/dash ${OUTDIR_EXT2}/bin/sh
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/bin/ls" \) ]; then
    echo "*** Building coreutils (target)"
    cd userland/coreutils-8.31
    make clean || true
    CFLAGS="--sysroot ${SYSROOT} -DOK_TO_USE_1S_CLOCK" ./configure --host=${TARGET} --prefix=${OUTDIR_EXT2}
    make ${MAKE_ARGS}
    make install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/sbin/init" \) ]; then
    echo "*** Building init (target)"
    rm -rf build/init
    mkdir -p build/init
    cd build/init
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR_EXT2} ../../userland/init
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/bin/ps" \) ]; then
    echo "*** Building ps (target)"
    rm -rf build/ps
    mkdir -p build/ps
    cd build/ps
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR_EXT2} ../../userland/ps
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/usr/bin/strace" \) ]; then
    echo "*** Building strace (target)"
    rm -rf build/strace
    mkdir -p build/strace
    cd build/strace
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR_EXT2} ../../userland/strace
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/usr/bin/ld" \) ]; then
    echo "*** Building binutils (target)"
    rm -rf build/binutils-target
    mkdir -p build/binutils-target
    cd build/binutils-target
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/binutils-${BINUTILS_VERSION}/configure --host=${TARGET} --target=${TARGET} --disable-nls --disable-werror --prefix=/usr
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install DESTDIR=${OUTDIR_EXT2}
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

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/usr/bin/gcc" \) ]; then
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
    make ${MAKE_ARGS} install DESTDIR=${OUTDIR_EXT2}
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/usr/lib/libc.a" \) ]; then
    echo "*** Building newlib (target)"
    rm -rf build/newlib-target
    mkdir build/newlib-target
    cd build/newlib-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR_EXT2} ../../lib/newlib-3.1.0
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/usr/include/dogfood/types.h" \) ]; then
    echo "*** Installing kernel headers (target)"
    rm -rf build/headers-target
    mkdir build/headers-target
    cd build/headers-target
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DCMAKE_INSTALL_PREFIX=${OUTDIR_EXT2} ../../kernel-headers
    ninja install
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$BUILD_TARGET" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_EXT2}/usr/bin/gdb" \) ]; then
    echo "*** Building gdb (target)"
    rm -rf build/gdb-target
    mkdir -p build/gdb-target
    cd build/gdb-target
    CFLAGS="--sysroot ${SYSROOT}" ../../userland/gdb-${GDB_VERSION}/configure --host=${TARGET} --target=${TARGET} --prefix=/usr --with-libgmp-prefix=${SYSROOT}
   CFLAGS="--sysroot /tmp/dogfood-sysroot" ../../userland/gdb-12.1/configure --host=x86_64-elf-dogfood --target=x86_64-elf-dogfood --prefix=/usr --with-libgmp-prefix=/tmp/dogfood-sysroot
    make ${MAKE_ARGS}
    make ${MAKE_ARGS} install DESTDIR=${OUTDIR_EXT2}
    cd ../..
    TARGET_DIRTY=1
fi

if [ "$TARGET_DIRTY" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_IMAGES}/ext2.img" \) ]; then
    echo "*** Creating ext2 image..."
    rm -f ${OUTDIR_IMAGES}/ext2.img
    mkfs.ext2 -d ${OUTDIR_EXT2} ${OUTDIR_IMAGES}/ext2.img 768m
    IMAGE_DIRTY=1
fi

if [ "$LOADER_DIRTY" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_IMAGES}/efi.img" \) ]; then
    echo "*** Creating EFI image..."
    EFI_IMAGE=${OUTDIR_IMAGES}/efi.img
    dd if=/dev/zero of=${EFI_IMAGE} bs=1M count=32
    mformat -i ${EFI_IMAGE} ::/
    mcopy -i ${EFI_IMAGE} -s ${OUTDIR_EFI}/* ::/
    IMAGE_DIRTY=1
fi

if [ "$IMAGE_DIRTY" -ne "0" -o \( "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_IMAGES}/dogfood.img" \) ]; then
    echo "*** Creating Dogfood image..."
    if [ ! -f "${OUTDIR_IMAGES}/dogfood.img" ]; then
        dd if=/dev/zero of=${OUTDIR_IMAGES}/dogfood.img bs=1M count=1024
        # new gpt schema, make 2 partition, first is 32MB, change first partition to EFI system
        echo 'g\nn\n1\n2048\n+32M\nn\n2\n\n\nt\n1\n1\nw\n'|fdisk ${OUTDIR_IMAGES}/dogfood.img
    fi
    if [ "$LOADER_DIRTY" -ne "0" ]; then
        EFI_SECTOR=`fdisk -l ${OUTDIR_IMAGES}/dogfood.img|grep "EFI System"|awk '{print $2*512}'`
        dd if=${OUTDIR_IMAGES}/efi.img of=${OUTDIR_IMAGES}/dogfood.img bs=${EFI_SECTOR} seek=1 conv=notrunc
    fi
    if [ "$TARGET_DIRTY" -ne "0" ]; then
        EXT2_SECTOR=`fdisk -l ${OUTDIR_IMAGES}/dogfood.img|grep "Linux filesystem"|awk '{print $2*512}'`
        dd if=${OUTDIR_IMAGES}/ext2.img of=${OUTDIR_IMAGES}/dogfood.img bs=${EXT2_SECTOR} seek=1 conv=notrunc
    fi
fi

if [ "${ONLY_REQUESTED_TARGETS}" -eq "0" -a ! -f "${OUTDIR_IMAGES}/run.sh" ]; then
    echo "qemu-system-x86_64 -bios /usr/share/qemu/OVMF.fd -drive format=raw,file=dogfood.img,if=ide,media=disk -serial stdio \$@" > ${OUTDIR_IMAGES}/run.sh
    chmod 755 ${OUTDIR_IMAGES}/run.sh
fi
