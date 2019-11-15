project(kernel-headers C)

set(HEADERS_DOGFOOD
    include/dogfood/fcntl.h
    include/dogfood/stat.h
    include/dogfood/syscall.h
    include/dogfood/types.h
    include/dogfood/vmop.h
)

install(FILES ${HEADERS_DOGFOOD} DESTINATION usr/include/dogfood)
