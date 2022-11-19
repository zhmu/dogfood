/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2022 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <dogfood/termios.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define TCSANOW 1
#define TCSADRAIN 2
#define TCSAFLUSH 3

int tcdrain(int fd);
int tcflow(int fildes, int action);
int tcflush(int fildes, int queue_selector);
int tcgetattr(int fildes, struct termios* termios_p);
pid_t tcgetpgrp(int fildes);
int tcsendbreak(int fildes, int duration);
int tcsetattr(int fildes, int optional_actions, const struct termios* termios_p);
int tcsetpgrp(int fildes, pid_t pgid);
int cfsetispeed(struct termios* termios_p, speed_t speed);
int cfsetospeed(struct termios* termios_p, speed_t speed);
speed_t cfgetispeed(const struct termios* termios_p);
speed_t cfgetospeed(const struct termios* termios_p);

#define TCOOFF 1
#define TCOON 2
#define TCIOFF 3
#define TCION 4

int tcflow(int fildes, int action);

#define TCIFLUSH 1
#define TCOFLUSH 2
#define TCIOFLUSH 3

int tcflush(int fildes, int queue_selector);

int tcsendbreak(int fildes, int duration);

__END_DECLS
