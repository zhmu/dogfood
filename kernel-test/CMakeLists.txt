project(kernel-tests CXX)
cmake_minimum_required(VERSION 3.9)

include(ExternalProject)

set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../kernel")
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../kernel-headers/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 ${COMMON_FLAGS}")

if(BUILD_KERNEL_TESTS)
    message("building kernel test ${COMMON_LIB}")
    add_subdirectory(googletest)
    add_definitions(-DBUILDING_TESTS)
    add_executable(kernel-test test-bio.cpp test-page.cpp stub.cpp)
    target_link_libraries(kernel-test ${COMMON_LIB} gtest_main)
else()
    message("configuring kernel test")
if(0)
    set(KERNELTEST_DIR ${PROJECT_BINARY_DIR}/kernel-test)
    file(MAKE_DIRECTORY ${KERNELTEST_DIR})
    execute_process(
        WORKING_DIRECTORY ${KERNELTEST_DIR}
        COMMAND ${CMAKE_COMMAND}
        -GNinja
        -DCMAKE_BUILD_TYPE=Release
        -DBUILD_KERNEL_TESTS=1
        ${CMAKE_CURRENT_SOURCE}
    )

    add_custom_target(kernel-test
        COMMAND ${CMAKE_COMMAND}
        --build ${KERNELTEST_DIR}
        --config Release
        COMMAND "Building kernel tests using native compiler"
        VERBATIM
    )
endif()

    ExternalProject_Add(kernel-tests
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        INSTALL_COMMAND ""
        CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DBUILD_KERNEL_TESTS=1
        -DCOMMON_LIB=$<TARGET_FILE:kernel_common>
    )
endif()
