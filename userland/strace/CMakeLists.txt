cmake_minimum_required(VERSION 3.9)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

add_definitions(-std=c++23)
add_executable(strace strace.cpp)
install(TARGETS strace DESTINATION usr/bin)
target_link_libraries(strace -static)
