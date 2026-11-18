/* Shared by the Mac OS 9 platform files. */
#ifndef ZSNES_MACOS9_H
#define ZSNES_MACOS9_H

#include <stdint.h>

/* posix.c */
uint64_t macos9_microseconds(void);
void macos9_close_log(void);

/* toolbox.c */
void macos9_init(void);     /* the Toolbox managers, once */
void macos9_idle(void);     /* let the system and other applications run */
void macos9_shutdown(void); /* stop sound, restore the menu bar */

#endif
