#!/bin/sh -e

. ./settings.sh

MAKE_ARGS=

# try to auto-detect the number of cpus in this system
if [ -f /proc/cpuinfo ]; then
    NCPUS=`grep processor /proc/cpuinfo|wc -l`
    MAKE_ARGS="${MAKE_ARGS} -j${NCPUS}"
fi

TOOLCHAIN=`realpath ${TOOLCHAIN}`

export PATH=${TOOLCHAIN}/bin:${PATH}

echo "*** Building binutils (toolchain)"
rm -rf build/binutils
mkdir -p build/binutils
cd build/binutils
../../userland/binutils-${BINUTILS_VERSION}/configure --target=${TARGET} --disable-nls --disable-werror --prefix=${TOOLCHAIN} --with-sysroot=${SYSROOT}
make ${MAKE_ARGS}
make ${MAKE_ARGS} install
cd ../..

echo "*** Building gcc (toolchain)"
rm -rf build/gcc
mkdir -p build/gcc
cd build/gcc
../../userland/gcc-${GCC_VERSION}/configure --target=${TARGET} --disable-nls --with-newlib --without-headers --enable-languages='c,c++' --prefix=${TOOLCHAIN} --disable-libstdcxx --disable-build-with-cxx --disable-libssp --disable-libquadmath --with-sysroot=${SYSROOT} --with-gxx-include-dir=${SYSROOT}/usr/include/c++/${GCC_VERSION}
make ${MAKE_ARGS}
make ${MAKE_ARGS} install
cd ../..

echo "set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSROOT ${SYSROOT})
set(CMAKE_WARN_DEPRECATED OFF)
include(CMakeForceCompiler)
CMAKE_FORCE_C_COMPILER(${TARGET}-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(${TARGET}-g++ GNU)

set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})
set(CMAKE_ASM_LINKER \${CMAKE_C_COMPILER})
" >> ${TOOLCHAIN_FILE}
