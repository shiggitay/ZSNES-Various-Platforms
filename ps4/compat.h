/* Included ahead of every file of the PS4 build (-include). The PS4 keeps a
   program from changing its current directory (chdir fails), which ZSNES
   works from: these keep one of their own (ps4/posix.c), used for the
   names ZSNES gives relative to it. */
#ifndef ZSNES_PS4_COMPAT_H
#define ZSNES_PS4_COMPAT_H

#include <stddef.h>

int ps4_chdir(char const* path);
char* ps4_getcwd(char* buf, size_t size);
void ps4_set_cwd(char const* path);

#define chdir ps4_chdir
#define getcwd ps4_getcwd

#endif
