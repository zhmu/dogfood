#ifndef __SYS_UN_H__
#define __SYS_UN_H__

#define SUN_PATH_MAX_LENGTH 127

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[SUN_PATH_MAX_LENGTH + 1];
};

#endif // __SYS_UN_H__
