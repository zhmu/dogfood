/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <dogfood/tty.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

int tcdrain(int fildes)
{
    return ioctl(fildes, TIOCDRAIN, 0);
}

static int get_flow_char(int fildes, int action, char* c)
{
    struct termios tios;
    if (tcgetattr(fildes, &tios) < 0) return -1;

    if (action == TCION) {
        *c = tios.c_cc[VSTART];
        return 0;
    }
    if (action == TCIOFF) {
        *c = tios.c_cc[VSTOP];
        return 0;
    }

    errno = EINVAL;
    return -1;
}

int tcflow(int fildes, int action)
{
    switch(action)
    {
        case TCOOFF:
            return ioctl(fildes, TIOCSTOP, 0);
        case TCOON:
            return ioctl(fildes, TIOCSTART, 0);
        case TCIOFF:
        case TCION:
        {
            char c;
            if (get_flow_char(fildes, action, &c) < 0) return -1;
            if (write(fildes, &c, sizeof(c)) != sizeof(c)) return -1;
            return 0;
        }
    }

    errno = EINVAL;
    return -1;
}

int tcflush(int fildes, int queue_selector)
{
    switch(queue_selector)
    {
        case TCIFLUSH:
            return ioctl(fildes, TIOCFLUSHR, 0);
        case TCOFLUSH:
            return ioctl(fildes, TIOCFLUSHW, 0);
        case TCIOFLUSH:
            return ioctl(fildes, TIOCFLUSHRW, 0);
    }

    errno = EINVAL;
    return -1;
}

int tcgetattr(int fildes, struct termios* termios_p)
{
    return ioctl(fildes, TIOCGETA, termios_p);
}

pid_t tcgetpgrp(int fildes) { return ioctl(fildes, TIOCGPGRP); }


int tcsendbreak(int fildes, int duration)
{
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = 250000000; // 0.25 sec

    if (ioctl(fildes, TIOCSBRK, 0) < 0) return -1;
    nanosleep(&tv, NULL);
    if (ioctl(fildes, TIOCCBRK, 0) < 0) return -1;
    return 0;
}

int tcsetattr(int fildes, int optional_actions, const struct termios* termios_p)
{
    switch(optional_actions) {
        case TCSANOW:
            return ioctl(fildes, TIOCSETA, termios_p);
        case TCSADRAIN:
            return ioctl(fildes, TIOCSETW, termios_p);
        case TCSAFLUSH:
            return ioctl(fildes, TIOCSETWF, termios_p);
    }

    errno = EINVAL;
    return -1;
}

int tcsetpgrp(int fildes, pid_t pgid) { return ioctl(fildes, TIOCSPGRP, pgid); }

int cfsetispeed(struct termios* termios_p, speed_t speed)
{
    termios_p->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios* termios_p, speed_t speed)
{
    termios_p->c_ospeed = speed;
    return 0;
}

speed_t cfgetispeed(const struct termios* termios_p)
{
    return termios_p->c_ispeed;
}

speed_t cfgetospeed(const struct termios* termios_p)
{
    return termios_p->c_ospeed;
}
