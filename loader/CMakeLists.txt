cmake_minimum_required(VERSION 3.12)
project(loader C CXX ASM)

set(COMMON_FLAGS "-Wno-error=pragmas -mno-red-zone -mno-avx -fpic  -g -O2 -Wall -Wextra -fshort-wchar -fno-strict-aliasing -ffreestanding -fno-stack-protector -fno-stack-check -fno-stack-check -fno-merge-all-constants -DCONFIG_x86_64 -DGNU_EFI_USE_MS_ABI -maccumulate-outgoing-args")
# not Werror
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS} -std=c11")
set(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=c11")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=c++20 -fno-exceptions -fno-rtti")

set(GNUEFI_ROOT 3rd-party/gnu-efi-3.0.15)
set(LIBEFI_SOURCES
    ${GNUEFI_ROOT}/lib/boxdraw.c
    ${GNUEFI_ROOT}/lib/smbios.c
    ${GNUEFI_ROOT}/lib/console.c
    ${GNUEFI_ROOT}/lib/crc.c
    ${GNUEFI_ROOT}/lib/data.c
    ${GNUEFI_ROOT}/lib/debug.c
    ${GNUEFI_ROOT}/lib/dpath.c
    ${GNUEFI_ROOT}/lib/error.c
    ${GNUEFI_ROOT}/lib/event.c
    ${GNUEFI_ROOT}/lib/exit.c
    ${GNUEFI_ROOT}/lib/guid.c
    ${GNUEFI_ROOT}/lib/hand.c
    ${GNUEFI_ROOT}/lib/hw.c
    ${GNUEFI_ROOT}/lib/init.c
    ${GNUEFI_ROOT}/lib/lock.c
    ${GNUEFI_ROOT}/lib/misc.c
    ${GNUEFI_ROOT}/lib/pause.c
    ${GNUEFI_ROOT}/lib/print.c
    ${GNUEFI_ROOT}/lib/sread.c
    ${GNUEFI_ROOT}/lib/str.c
    ${GNUEFI_ROOT}/lib/cmdline.c
    ${GNUEFI_ROOT}/lib/runtime/rtlock.c
    ${GNUEFI_ROOT}/lib/runtime/efirtlib.c
    ${GNUEFI_ROOT}/lib/runtime/rtstr.c
    ${GNUEFI_ROOT}/lib/runtime/vm.c
    ${GNUEFI_ROOT}/lib/runtime/rtdata.c
    ${GNUEFI_ROOT}/lib/x86_64/initplat.c
    ${GNUEFI_ROOT}/lib/x86_64/math.c
    ${GNUEFI_ROOT}/lib/x86_64/setjmp.S
    ${GNUEFI_ROOT}/lib/x86_64/callwrap.c
    ${GNUEFI_ROOT}/lib/x86_64/efi_stub.S
)
add_library(efi ${LIBEFI_SOURCES})
target_include_directories(efi PUBLIC ${GNUEFI_ROOT}/inc PRIVATE ${GNUEFI_ROOT}/lib)

set(LIBGNUEFI_SOURCES
    ${GNUEFI_ROOT}/gnuefi/crt0-efi-x86_64.S
    ${GNUEFI_ROOT}/gnuefi/reloc_x86_64.c
)

add_library(gnuefi ${LIBGNUEFI_SOURCES})
target_include_directories(gnuefi PUBLIC ${GNUEFI_ROOT}/inc PRIVATE ${GNUEFI_ROOT}/gnuefi ${GNUEFI_ROOT}/inc/x86_64 ${GNUEFI_ROOT}/inc/protocol)
set(LOADER_SOURCES
    ${GNUEFI_ROOT}/gnuefi/crt0-efi-x86_64.S
    ${GNUEFI_ROOT}/gnuefi/reloc_x86_64.c
    src/loader.cpp
    src/heap.cpp
    src/memory.cpp
    src/bio.cpp
    src/ext2.cpp
    src/fs.cpp
    src/lib.cpp
    src/kernel.cpp
)
add_executable(loader_elf ${LOADER_SOURCES})

#set(CMAKE_C_LINK_EXECUTABLE "ld -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 -shared -Bsymbolic -T ${CMAKE_SOURCE_DIR}/${GNUEFI_ROOT}/gnuefi/elf_x86_64_efi.lds <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "ld -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 -shared -Bsymbolic -T ${CMAKE_SOURCE_DIR}/${GNUEFI_ROOT}/gnuefi/elf_x86_64_efi.lds <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
# -lefi -lgnuefi /usr/lib/gcc/x86_64-linux-gnu/12/libgcc.a -T /tmp/gnu-efi-3.0.15//apps/../gnuefi/elf_x86_64_efi.lds")
#<CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")

#ld -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 -shared -Bsymbolic -L../lib -L../gnuefi ../gnuefi/crt0-efi-x86_64.o printenv.o -o printenv.so -lefi -lgnuefi /usr/lib/gcc/x86_64-linux-gnu/12/libgcc.a -T /tmp/gnu-efi-3.0.15//apps/../gnuefi/elf_x86_64_efi.lds
#set(CMAKE_C_LINK_FLAGS "-nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 -shared -Bsymbolic -L../lib -L../gnuefi ../gnuefi/crt0-efi-x86_64.o printenv.o -o printenv.so -lefi -lgnuefi /usr/lib/gcc/x86_64-linux-gnu/12/libgcc.a -T /tmp/gnu-efi-3.0.15//apps/../gnuefi/elf_x86_64_efi.lds")
#set(CMAKE_C_LINK_FLAGS "-nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 -shared -Bsymbolic -T /tmp/gnu-efi-3.0.15//apps/../gnuefi/elf_x86_64_efi.lds")
#-b elf64-x86-64 -T ${CMAKE_CURRENT_SOURCE_DIR}/x86_64/ldscript -nostdlib -nodefaultlibs")
target_link_libraries(loader_elf efi)
set_target_properties(loader_elf PROPERTIES OUTPUT_NAME "loader.elf")

add_custom_command(
    OUTPUT loader.efi
    COMMENT "Converting loader ELF to PE32+ EFI"
    DEPENDS loader_elf
    COMMAND objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* -j .reloc --target efi-app-x86_64 $<TARGET_FILE:loader_elf> ${CMAKE_CURRENT_BINARY_DIR}/loader.efi
)
add_custom_target(loader_efi DEPENDS loader.efi)

add_custom_target(loader ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/loader.efi)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/loader.efi DESTINATION .)

