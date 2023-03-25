cmake_minimum_required(VERSION 3.12)
project(loader C CXX ASM)

set(COMMON_FLAGS "-Wno-error=pragmas -mno-red-zone -mno-avx -fpic  -g -O2 -Wall -Wextra -fshort-wchar -fno-strict-aliasing -ffreestanding -fno-stack-protector -fno-stack-check -fno-stack-check -fno-merge-all-constants -DGNU_EFI_USE_MS_ABI -maccumulate-outgoing-args")
# not Werror
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS} -std=c11")
set(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=c11")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=c++20 -fno-exceptions -fno-rtti")

set(GNUEFI_ROOT 3rd-party/gnu-efi-3.0.15)
add_subdirectory(${GNUEFI_ROOT})

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

set(CMAKE_CXX_LINK_EXECUTABLE "ld -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 -shared -Bsymbolic -T ${CMAKE_SOURCE_DIR}/${GNUEFI_ROOT}/gnuefi/elf_x86_64_efi.lds <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

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
