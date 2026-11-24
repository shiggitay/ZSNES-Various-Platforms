/* posix.c - the current directory the PS4 build keeps for itself (see
   ps4/compat.h): the system's when chdir works, its own when it does not. */
#include "compat.h"

#undef chdir
#undef getcwd

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char cwd[1024];

void ps4_set_cwd(char const* const path)
{
    snprintf(cwd, sizeof cwd, "%s", path);
}

int ps4_chdir(char const* const path)
{
    char full[1024];
    struct stat st;

    if (!path || !*path) {
        errno = ENOENT;
        return -1;
    }
    if (path[0] == '/' || !cwd[0])
        snprintf(full, sizeof full, "%s", path);
    else
        snprintf(full, sizeof full, "%s/%s", strcmp(cwd, "/") ? cwd : "", path);
    if (stat(full, &st) != 0)
        return -1;
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    (void)chdir(full); /* where the system allows it */
    /* ".." and "." are left in: every system the paths reach takes them. */
    snprintf(cwd, sizeof cwd, "%s", full);
    return 0;
}

char* ps4_getcwd(char* const buf, size_t const size)
{
    if (!cwd[0] && getcwd(cwd, sizeof cwd) == NULL)
        snprintf(cwd, sizeof cwd, "/");
    if (!buf || strlen(cwd) + 1 > size) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, cwd, strlen(cwd) + 1);
    return buf;
}
