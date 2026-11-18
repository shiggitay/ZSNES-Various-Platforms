/* Directories for newlib's <dirent.h>, implemented by macos9/posix.c on the
   File Manager. */
#ifndef MACOS9_SYS_DIRENT_H
#define MACOS9_SYS_DIRENT_H
#include <sys/types.h>
struct dirent {
    ino_t d_ino;
    char d_name[256];
};
typedef struct mac_dir DIR;
struct stat;
int fstatat(int fd, char const* name, struct stat* st, int flag);
#endif
