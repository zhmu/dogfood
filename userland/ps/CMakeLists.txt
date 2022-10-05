cmake_minimum_required(VERSION 3.9)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

project(ps C)
add_executable(ps ps.c)
target_link_libraries(ps -static)

install(TARGETS ps DESTINATION bin)
