set(SOURCES
    entry.S
    exception.S
    initcode.S
    startup.cpp
)

add_library(kernel_x86_64 ${SOURCES})
