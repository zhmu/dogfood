#define O_CREAT (1 << 0)
#define O_RDONLY (1 << 1)
#define O_WRONLY (1 << 2)
#define O_RDWR (1 << 3)
#define O_APPEND (1 << 4)
#define O_EXCL (1 << 5)
#define O_TRUNC (1 << 6)
#define O_CLOEXEC (1 << 7)
#define O_NONBLOCK (1 << 8)
#define O_DIRECTORY (1 << 31)

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

#define FD_CLOEXEC 1

#define AT_FDCWD (-1)
#define AT_SYMLINK_NOFOLLOW (1 << 0)
