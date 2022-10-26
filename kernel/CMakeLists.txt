project(kernel ASM CXX)
cmake_minimum_required(VERSION 3.9)

set(COMMON_FLAGS "${COMMON_FLAGS} -fno-rtti -fno-exceptions -fno-pie")
set(COMMON_FLAGS "${COMMON_FLAGS} -fno-builtin -nostdlib")
set(COMMON_FLAGS "${COMMON_FLAGS} -m64 -march=athlon64 -mcmodel=large")
set(COMMON_FLAGS "${COMMON_FLAGS} -mno-red-zone -mno-sse -fno-stack-protector")
set(COMMON_FLAGS "${COMMON_FLAGS} -O2 -g")
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}")
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../kernel-headers/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 ${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${COMMON_FLAGS}")

set(CMAKE_CXX_LINK_EXECUTABLE "ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_FLAGS "-b elf64-x86-64 -T ${CMAKE_CURRENT_SOURCE_DIR}/x86_64/ldscript -nostdlib -nodefaultlibs")

add_subdirectory(hw)
add_subdirectory(x86_64)

add_library(kernel_common lib.cpp page_allocator.cpp process.cpp vm.cpp bio.cpp ext2.cpp fs.cpp syscall.cpp exec.cpp file.cpp memory.cpp signal.cpp)

# kernel_elf glues all libraries together; due to the CMake being used, we need
# at least one source file which must be empty (we rely on x86_64's starting
# with the entry point as this ensures we don't need any specific knowledge in
# the multiboot stub)
add_executable(kernel_elf dummy.cpp)
target_link_libraries(kernel_elf --start-group kernel_x86_64 kernel_common kernel_hw --end-group)
set_target_properties(kernel_elf PROPERTIES OUTPUT_NAME "kernel.elf")

add_custom_command(
    OUTPUT kernel.bin
    COMMENT "Converting kernel ELF to binary"
    DEPENDS kernel_elf
    COMMAND objcopy -O binary $<TARGET_FILE:kernel_elf> ${CMAKE_CURRENT_BINARY_DIR}/kernel.bin
)
add_custom_target(kernel_bin DEPENDS kernel.bin)

add_custom_command(
    OUTPUT kernel.mb
    COMMENT "Merging kernel binary with multiboot loader"
    DEPENDS multiboot kernel.bin
    COMMAND cat $<TARGET_FILE:multiboot> kernel.bin > ${CMAKE_CURRENT_BINARY_DIR}/kernel.mb
)
add_custom_target(kernel_mb DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel.mb)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kernel.mb DESTINATION .)
