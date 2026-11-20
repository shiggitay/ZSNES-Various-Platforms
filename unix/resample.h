/* resample.h - the DSP's 32000 Hz to the sound card's rate (44100 Hz, as a
   CD), for the frontends whose sound path has no converter of its own worth
   the name: SDL 1.2 (unix/sdl12compat.c) and the Sound Manager
   (macos9/toolbox.c). Left to them, the conversion is done by the sound
   driver - on a Power Mac, Linux's dmasound or the Sound Manager - which
   picks the nearest sample or interpolates linearly, and that is heard as
   grit and whistling on the high notes. */
#ifndef ZSNES_RESAMPLE_H
#define ZSNES_RESAMPLE_H

#include <stdint.h>

#define ZRS_TAPS 16
#define ZRS_MAX_PHASES 1024

typedef struct zrs {
    unsigned channels;
    unsigned up, down; /* out_rate / in_rate, in lowest terms */
    unsigned acc;      /* phase: 0 .. up-1 */
    unsigned need;     /* input frames to take before the next output */
    int16_t hist[2][ZRS_TAPS];
    int16_t (*table)[ZRS_TAPS]; /* up phases */
} zrs;

/* 0 on success. Fails for rates whose ratio needs over ZRS_MAX_PHASES
   phases, or without memory; the caller then plays at in_rate. */
int zrs_init(zrs* r, unsigned in_rate, unsigned out_rate, unsigned channels);
void zrs_free(zrs* r);

/* The most output frames zrs_run can make from in_frames. */
unsigned zrs_max_out(zrs const* r, unsigned in_frames);

/* Converts in_frames interleaved frames to out (room for zrs_max_out),
   returning how many frames it wrote. Keeps its place between calls. */
unsigned zrs_run(zrs* r, int16_t const* in, unsigned in_frames, int16_t* out);

#endif
