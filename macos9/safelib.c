/* No child processes on classic Mac OS: the movie encoders ZSNES pipes to
   (mencoder, lame) do not exist there, and the callers fall back. */
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#include "../unix/safelib.h"

pid_t safe_fork(int* a, size_t size) { (void)a; (void)size; errno = ENOSYS; return -1; }
#undef popen
#undef pclose
FILE* safe_popen(char* command, const char* mode) { (void)command; (void)mode; errno = ENOSYS; return NULL; }
void safe_pclose(FILE* fp) { (void)fp; }
