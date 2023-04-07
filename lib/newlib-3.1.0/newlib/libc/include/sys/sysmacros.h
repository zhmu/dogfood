#ifndef _SYS_SYSMACROS_H_
#define _SYS_SYSMACROS_H_

dev_t makedev(unsigned int major, unsigned int minor);
unsigned int major(dev_t dev);
unsigned int minor(dev_t dev);

#endif /* _SYS_SYSMACROS_H_ */
