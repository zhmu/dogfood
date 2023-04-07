TARGET=x86_64-elf-dogfood
BINUTILS_VERSION=2.39
GCC_VERSION=12.2.0
GMP_VERSION=6.2.1
MPFR_VERSION=4.1.0
MPC_VERSION=1.2.1
GDB_VERSION=12.1
MAKE_VERSION=4.4
SED_VERSION=4.9

TOOLCHAIN_FILE=/opt/dogfood-toolchain/${TARGET}.txt
TOOLCHAIN=/opt/dogfood-toolchain # where toolchain items get written

SYSROOT=/tmp/dogfood-sysroot
