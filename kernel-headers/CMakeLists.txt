project(kernel-headers C)

set(HEADERS_DOGFOOD
    include/dogfood/errno.h
    include/dogfood/fcntl.h
    include/dogfood/procinfo.h
    include/dogfood/ptrace.h
    include/dogfood/stat.h
    include/dogfood/signal.h
    include/dogfood/syscall.h
    include/dogfood/termios.h
    include/dogfood/tty.h
    include/dogfood/types.h
    include/dogfood/vmop.h
    include/dogfood/user.h
    include/dogfood/utsname.h
    include/dogfood/wait.h
    include/dogfood/socket.h
    include/dogfood/netinet.h
)

install(FILES ${HEADERS_DOGFOOD} DESTINATION usr/include/dogfood)
