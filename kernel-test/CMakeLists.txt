project(kernel-tests CXX ASM)
cmake_minimum_required(VERSION 3.9)

include(ExternalProject)

set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../kernel")
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../kernel-headers/include")
set(COMMON_FLAGS "${COMMON_FLAGS} -no-pie")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 ${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${COMMON_FLAGS}")

if(BUILD_KERNEL_TESTS)
    message("building kernel test ${COMMON_LIB}")
    add_subdirectory(googletest)
    add_definitions(-DBUILDING_TESTS)
    set(KERNEL_SOURCES
        ../kernel/x86_64/exception.S
        ../kernel/x86_64/initcode.S
        ../kernel/x86_64/startup.cpp
    )
    set(SOURCES
        test-bio.cpp test-page.cpp test-ext2.cpp stub.cpp
    )
    add_executable(kernel-test ${KERNEL_SOURCES} ${SOURCES})
    target_link_libraries(kernel-test ${COMMON_LIB} gtest_main)
else()
    message("configuring kernel test")

    ExternalProject_Add(kernel-tests
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        INSTALL_COMMAND ""
        CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DBUILD_KERNEL_TESTS=1
        -DCOMMON_LIB=$<TARGET_FILE:kernel_common>
    )
endif()
