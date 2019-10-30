project(libm C)

# install a dummy libm to satisfy things that want to drag it in
# all stuff is already in libc
set(SOURCES
    dummy.c)

add_library(m STATIC ${SOURCES})
install(TARGETS m DESTINATION usr/lib)
