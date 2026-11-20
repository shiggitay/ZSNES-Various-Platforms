/* resample.c - see resample.h.

   A polyphase windowed-sinc filter, 16 taps, with a phase for each output
   position the ratio can produce (441 for 32000 to 44100 Hz), so there is
   no drift and no interpolation between phases. Coefficients are 14-bit
   integers and the sums 32-bit: integer work, which every PowerPC from the
   601 up does in a cycle or two, at about 1.4 million multiply-adds a
   second for stereo. The cut-off sits at 0.45 of the DSP's rate (14.4 kHz),
   above anything the SNES's own Gaussian interpolation lets through. */
#include "resample.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define HALF (ZRS_TAPS / 2)
#define SHIFT 14
#define CUTOFF 0.45 /* of the input rate */
#define BETA 7.0    /* Kaiser window: about 70 dB down */

static unsigned gcd(unsigned a, unsigned b)
{
    while (b) {
        unsigned const t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static double bessel_i0(double const x)
{
    double sum = 1, term = 1, y = x / 2;
    int k;

    for (k = 1; k < 50; k++) {
        term *= (y / k) * (y / k);
        sum += term;
        if (term < 1e-12 * sum)
            break;
    }
    return sum;
}

int zrs_init(zrs* const r, unsigned const in_rate, unsigned const out_rate, unsigned const channels)
{
    unsigned g, p;
    int t;

    memset(r, 0, sizeof *r);
    if (!in_rate || !out_rate || channels < 1 || channels > 2)
        return -1;
    g = gcd(in_rate, out_rate);
    r->up = out_rate / g;
    r->down = in_rate / g;
    r->channels = channels;
    if (r->up > ZRS_MAX_PHASES)
        return -1;
    r->table = (int16_t(*)[ZRS_TAPS])malloc(sizeof *r->table * r->up);
    if (!r->table)
        return -1;
    for (p = 0; p < r->up; p++) {
        /* The output falls between taps HALF-1 and HALF, frac of the way. */
        double const frac = (double)p / r->up;
        double h[ZRS_TAPS], sum = 0;
        int isum = 0, peak = 0;

        for (t = 0; t < ZRS_TAPS; t++) {
            double const x = (HALF - 1 + frac) - t;
            double const xn = x / HALF;
            double const s = x == 0 ? 2 * CUTOFF : sin(2 * M_PI * CUTOFF * x) / (M_PI * x);
            double const w = xn * xn >= 1 ? 0 : bessel_i0(BETA * sqrt(1 - xn * xn)) / bessel_i0(BETA);

            h[t] = s * w;
            sum += h[t];
        }
        for (t = 0; t < ZRS_TAPS; t++) {
            int const c = (int)floor(h[t] / sum * (1 << SHIFT) + 0.5);

            r->table[p][t] = (int16_t)c;
            isum += c;
            if (abs(c) > abs(r->table[p][peak]))
                peak = t;
        }
        /* Exactly unity gain: rounding's remainder onto the largest tap. */
        r->table[p][peak] += (int16_t)((1 << SHIFT) - isum);
    }
    r->need = HALF; /* the first output waits for the filter to fill */
    return 0;
}

void zrs_free(zrs* const r)
{
    free(r->table);
    r->table = NULL;
}

unsigned zrs_max_out(zrs const* const r, unsigned const in_frames)
{
    return (unsigned)(((unsigned long long)in_frames * r->up + r->down - 1) / r->down) + 1;
}

static int16_t clip(int32_t const v)
{
    return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v;
}

unsigned zrs_run(zrs* const r, int16_t const* in, unsigned in_frames, int16_t* out)
{
    unsigned const ch = r->channels;
    unsigned made = 0, c;
    int t;

    for (;;) {
        while (r->need) {
            if (!in_frames)
                return made;
            for (c = 0; c < ch; c++) {
                memmove(&r->hist[c][0], &r->hist[c][1], (ZRS_TAPS - 1) * sizeof(int16_t));
                r->hist[c][ZRS_TAPS - 1] = in[c];
            }
            in += ch;
            in_frames--;
            r->need--;
        }
        {
            int16_t const* const k = r->table[r->acc];

            for (c = 0; c < ch; c++) {
                int16_t const* const x = r->hist[c];
                int32_t s = 1 << (SHIFT - 1);

                for (t = 0; t < ZRS_TAPS; t++)
                    s += (int32_t)k[t] * x[t];
                *out++ = clip(s >> SHIFT);
            }
        }
        made++;
        r->acc += r->down;
        while (r->acc >= r->up) {
            r->acc -= r->up;
            r->need++;
        }
    }
}
