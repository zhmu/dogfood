project(kernel ASM CXX)
cmake_minimum_required(VERSION 3.9)

set(COMMON_FLAGS "${COMMON_FLAGS} -fno-rtti -fno-exceptions -fno-pie")
set(COMMON_FLAGS "${COMMON_FLAGS} -fno-builtin -nostdlib")
set(COMMON_FLAGS "${COMMON_FLAGS} -m64 -march=athlon64 -mcmodel=large")
set(COMMON_FLAGS "${COMMON_FLAGS} -mno-red-zone -mno-sse -fno-stack-protector")
set(COMMON_FLAGS "${COMMON_FLAGS} -O2 -g")
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}")
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../kernel-headers/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 ${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${COMMON_FLAGS}")

add_subdirectory(hw)
add_subdirectory(x86_64)

add_library(kernel_common lib.cpp page_allocator.cpp process.cpp vm.cpp bio.cpp ext2.cpp fs.cpp syscall.cpp exec.cpp file.cpp memory.cpp signal.cpp ptrace.cpp pipe.cpp select.cpp partition.cpp device.cpp)

set(CMAKE_CXX_LINK_EXECUTABLE "ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_FLAGS "-b elf64-x86-64 -T ${CMAKE_CURRENT_SOURCE_DIR}/x86_64/ldscript -nostdlib -nodefaultlibs")

add_executable(kernel dummy.cpp)
target_link_libraries(kernel --start-group kernel_x86_64 kernel_common kernel_hw --end-group)
set_target_properties(kernel PROPERTIES OUTPUT_NAME "kernel.elf")
