cmake_minimum_required(VERSION 3.9)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

project(init C)
add_executable(init init.c)
target_link_libraries(init -static)

install(TARGETS init DESTINATION sbin)
