/* Included ahead of every file of the PS4 build (-include). The PS4 keeps a
   program from changing its current directory (chdir fails), which ZSNES
   works from: these keep one of their own (ps4/posix.c), used for the
   names ZSNES gives relative to it. */
#ifndef ZSNES_PS4_COMPAT_H
#define ZSNES_PS4_COMPAT_H

#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

int ps4_chdir(char const* path);
char* ps4_getcwd(char* buf, size_t size);
void ps4_set_cwd(char const* path);
void ps4_log(char const* fmt, ...);

/* Directories are read with the PS4's own calls (sceKernelOpen and
   sceKernelGetdents): each remembers its whole path, an entry is looked up
   by its whole path, and an entry the PS4 will not stat is still listed by
   the type the directory gives it. stat falls back to sceKernelStat. */
DIR* ps4_opendir(char const* path);
struct dirent* ps4_readdir(DIR* dir);
int ps4_closedir(DIR* dir);
int ps4_dirfd(DIR* dir);
int ps4_fstatat(int fd, char const* name, struct stat* st, int flag);
int ps4_stat(char const* path, struct stat* st);
/* The C library's realpath looks the path up through /proc, which the PS4
   has none of: it failed for every ROM. This one works on the path's text
   (from the current directory, "." and ".." taken out) and checks that the
   result exists. fopen takes names from the current directory and logs
   what it cannot open. */
char* ps4_realpath(char const* path, char* resolved);
FILE* ps4_fopen(char const* path, char const* mode);
/* access: whether the path exists, by sceKernelStat (the C library's
   failed, and the load dialog checks every ROM with it before loading). */
int ps4_access(char const* path, int mode);

#define chdir ps4_chdir
#define getcwd ps4_getcwd
#define opendir ps4_opendir
#define readdir ps4_readdir
#define closedir ps4_closedir
#undef dirfd
#define dirfd ps4_dirfd
#define fstatat ps4_fstatat
#define stat(path, st) ps4_stat(path, st)
#define realpath(path, resolved) ps4_realpath(path, resolved)
#define fopen(path, mode) ps4_fopen(path, mode)
#define access(path, mode) ps4_access(path, mode)

#endif
