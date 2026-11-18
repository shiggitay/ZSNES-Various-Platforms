/*
 * The DSP voice mixers as register machines, included by cpu/c_dspproc.c.
 *
 * Textual include: the includer supplies the integer typedefs, the DSP globals
 * and the DSPInterpolate pointer.
 *
 * Each w_<name> has c_dspproc.c's `mixfn` ABI: reads the voice, DSP-buffer
 * index (*pesi), increment (*pebx) and decoded samples (edi); advances *pesi,
 * and *pebx in the pitch-modulation variants. static inline so a partial
 * includer draws no unused-function warnings.
 */

#include "../unaligned.h"
#ifndef DSP_MIXERS_H
#define DSP_MIXERS_H

/* A 32-bit x86 register as the mixers use it: the full value and, kept
 * separately, the low word the volume lookup leaves in cx. The assembly's byte
 * and word sub-registers are spelled out as shifts below rather than aliased
 * through a union, whose layout would only match on a little-endian host. */
typedef struct {
    u4 e;
    u2 w;
} x86reg;

/* The envelope level: byte 2 of Voice0EnvInc, `[Voice0EnvInc+ebp*4+2]`. */
static inline u1 mix_envbyte(u4 const voice)
{
    return (u1)(Voice0EnvInc[voice] >> 16);
}

/* ProcessPMod ebp: a side-effect that stores one byte into PModBuffer[esi].
 * Preserves the caller's ecx/edx (taken by value). */
static inline void mix_ProcessPMod(u4 ebp, u4 esi, s2* edi, x86reg edx, x86reg ecx)
{
    u2 const cx = (u2)((ecx.w & 0xFF00u) | mix_envbyte(ebp)); /* mov cl,[Voice0EnvInc+ebp*4+2] */
    u2 ax = ld16u((u1*)edi + edx.e * 2); /* mov ax,[edi+edx*2] */
    u1 dl;
    { /* imul cx -> dx:ax = ax * cx */
        s4 p = (s2)ax * (s2)cx;
        ax = (u2)p;
        dl = (u1)(p >> 16);
    }
    ax >>= 7; /* shr ax,7 */
    dl = (u1)(dl + dl); /* add dl,dl */
    PModBuffer[esi] = (u1)((ax >> 8) | dl); /* or ah,dl; mov [PModBuffer+esi],ah */
}

/* Volume lookup: index VolumeConvTable by (envinc<<8 | volbyte). */
static inline u2 mix_vconv(u4 voice, u1 volbyte)
{
    u4 const idx = (u4)volbyte | (u4)mix_envbyte(voice) << 8;
    return ld16u((u1*)VolumeConvTable + idx * 2);
}

/* `mov edx,[BRRPlace0+ebp*8+3]`: the top byte of the position and the low
 * three of the dword after it, i.e. the whole-sample index. */
static inline u4 mix_brrpos(u4 const voice)
{
    return BRRPlace0[voice][0] >> 24 | BRRPlace0[voice][1] << 8;
}

/* Volume scale of one sample: (sample * volconv) >> 7, truncated to 16 bits.
 * The asm's `imul cx; shr ax,7; add dl,dl; or ah,dl; movsx eax,ax` reconstructs
 * exactly this (proven by the diff-tests). */
static inline s4 mix_scale(s2 sample, u2 volconv)
{
    s4 p = (s2)sample * (s2)volconv;
    return (s2)(p >> 7);
}

/* Fetch this voice's next sample: noise, or the decoded sample (with a
 * ProcessPMod side-effect on the way when a following voice pitch-modulates). */
static inline s2 mix_sample(u4 voice, u4 esi, s2* edi, x86reg edx, x86reg ecx)
{
    if (UniqueSoundv != 0) {
        if (DSPMem[0x3D] & (u1)powhack) { /* test DSPMem[3D],al / jnz -> noise */
            NoisePointer += NoiseInc;
            return (s2)ld16le(NoiseData + (NoisePointer >> 18) * 2); /* a byte table */
        }
        mix_ProcessPMod(voice, esi, edi, edx, ecx);
    }
    return *(s2*)((u1*)edi + edx.e * 2);
}

/* Interpolated fetch: like mix_sample, but the sample comes from DSPInterpolate
 * (noise still bypasses interpolation). The interpolated mixers never load cx
 * before their ProcessPMod, so it runs with ch=0 - a defined replacement for
 * the asm's undefined ch (dispatcher register residue). */
static inline s2 mix_sample_interp(u4 voice, u4 esi, s2* edi, x86reg edx)
{
    if (UniqueSoundv != 0) {
        if (DSPMem[0x3D] & (u1)powhack) {
            NoisePointer += NoiseInc;
            return (s2)ld16le(NoiseData + (NoisePointer >> 18) * 2); /* a byte table */
        }
        x86reg ecx;
        ecx.e = 0; /* ch = 0 */
        mix_ProcessPMod(voice, esi, edi, edx, ecx);
    }
    return (s2)DSPInterpolate(edx.e, voice);
}

/* CalculatePMod ebp: the pitch-modulated increment for the next sample. */
static inline u4 mix_CalculatePMod(u4 voice, u4 esi)
{
    u1 m = (u1)(PModBuffer[esi] + 0x80); /* movzx + add al,80h */
    unsigned long long p = (unsigned long long)m * Voice0Freq[voice]; /* mul ebx */
    return (u4)(p >> 7); /* shr eax,7 | shl edx,25 */
}

/* Common prologue: load the sample position and the first channel's volconv,
 * then fetch the sample. */
#define MIX_FETCH(voice, esi, edi, volbyte, sample, ecx) \
    u4 esi = *pesi;                                      \
    x86reg edx;                                          \
    edx.e = mix_brrpos(voice);                           \
    x86reg ecx;                                          \
    ecx.e = 0;                                           \
    ecx.w = mix_vconv((voice), (volbyte));               \
    s2 sample = mix_sample((voice), esi, edi, edx, ecx)

/* Non-pitch-mod tail: advance esi and step BRRPlace0 by the frequency. */
#define MIX_TAIL(voice, esi)      \
    esi += 2;                     \
    BRRPlace0[voice][0] += *pebx; \
    *pesi = esi

/* Frequency-reload tail: variants that clobbered ebx to hold the sample reload
 * Voice0Freq for the BRRPlace0 step (mov ebx,[Voice0Freq+ebp*4]) and leave it
 * in *pebx. */
#define MIX_TAIL_FREQ(voice, esi) \
    esi += 2;                     \
    *pebx = Voice0Freq[voice];    \
    BRRPlace0[voice][0] += *pebx; \
    *pesi = esi

/* Pitch-mod tail: advance esi, recompute the increment, step BRRPlace0. */
#define MIX_TAIL_PM(voice, esi)                  \
    esi += 2;                                    \
    {                                            \
        u4 nb = mix_CalculatePMod((voice), esi); \
        BRRPlace0[voice][0] += nb;               \
        *pebx = nb;                              \
    }                                            \
    *pesi = esi

static inline void w_NonEchoMono(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0Volume[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 2) += mix_scale(s, ecx.w);
    MIX_TAIL(voice, esi);
}

static inline void w_NonEchoStereo(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0VolumeR[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 4) += mix_scale(s, ecx.w);
    *(s4*)((u1*)DSPBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeL[voice]));
    MIX_TAIL(voice, esi);
}

static inline void w_EchoMono(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0Volume[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 2) += mix_scale(s, ecx.w);
    *(s4*)((u1*)EchoBuffer + esi * 2) += mix_scale(s, mix_vconv(voice, Voice0Volumee[voice]));
    MIX_TAIL(voice, esi);
}

static inline void w_EchoStereo(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0VolumeR[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 4) += mix_scale(s, ecx.w);
    *(s4*)((u1*)EchoBuffer + esi * 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeRe[voice]));
    *(s4*)((u1*)DSPBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeL[voice]));
    *(s4*)((u1*)EchoBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeLe[voice]));
    /* This variant reused ebx to hold the sample, so it reloads Voice0Freq. */
    MIX_TAIL_FREQ(voice, esi);
}

static inline void w_NonEchoMonoPM(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0Volume[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 2) += mix_scale(s, ecx.w);
    MIX_TAIL_PM(voice, esi);
}

static inline void w_NonEchoStereoPM(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0VolumeR[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 4) += mix_scale(s, ecx.w);
    *(s4*)((u1*)DSPBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeL[voice]));
    MIX_TAIL_PM(voice, esi);
}

static inline void w_EchoMonoPM(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0Volume[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 2) += mix_scale(s, ecx.w);
    *(s4*)((u1*)EchoBuffer + esi * 2) += mix_scale(s, mix_vconv(voice, Voice0Volumee[voice]));
    MIX_TAIL_PM(voice, esi);
}

static inline void w_EchoStereoPM(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    MIX_FETCH(voice, esi, edi, Voice0VolumeR[voice], s, ecx);
    *(s4*)((u1*)DSPBuffer + esi * 4) += mix_scale(s, ecx.w);
    *(s4*)((u1*)EchoBuffer + esi * 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeRe[voice]));
    *(s4*)((u1*)DSPBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeL[voice]));
    *(s4*)((u1*)EchoBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeLe[voice]));
    MIX_TAIL_PM(voice, esi);
}

/* Interpolated variants: the non-interpolated channel layout, but the sample
 * comes from mix_sample_interp and all of them step BRRPlace0 by Voice0Freq
 * (MIX_TAIL_FREQ). EchoMonoInterpolated's asm left the decoded sample in ebx,
 * which is dead - the dispatcher reloads Voice0Freq first - so it returns
 * Voice0Freq here too. */
static inline void w_NonEchoMonoInterpolated(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    u4 esi = *pesi;
    x86reg edx;
    edx.e = mix_brrpos(voice);
    s2 s = mix_sample_interp(voice, esi, edi, edx);
    *(s4*)((u1*)DSPBuffer + esi * 2) += mix_scale(s, mix_vconv(voice, Voice0Volume[voice]));
    MIX_TAIL_FREQ(voice, esi);
}

static inline void w_NonEchoStereoInterpolated(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    u4 esi = *pesi;
    x86reg edx;
    edx.e = mix_brrpos(voice);
    s2 s = mix_sample_interp(voice, esi, edi, edx);
    *(s4*)((u1*)DSPBuffer + esi * 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeR[voice]));
    *(s4*)((u1*)DSPBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeL[voice]));
    MIX_TAIL_FREQ(voice, esi);
}

static inline void w_EchoMonoInterpolated(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    u4 esi = *pesi;
    x86reg edx;
    edx.e = mix_brrpos(voice);
    s2 s = mix_sample_interp(voice, esi, edi, edx);
    *(s4*)((u1*)DSPBuffer + esi * 2) += mix_scale(s, mix_vconv(voice, Voice0Volume[voice]));
    *(s4*)((u1*)EchoBuffer + esi * 2) += mix_scale(s, mix_vconv(voice, Voice0Volumee[voice]));
    MIX_TAIL_FREQ(voice, esi);
}

static inline void w_EchoStereoInterpolated(u4 voice, u4* const pesi, u4* const pebx, s2* edi)
{
    u4 esi = *pesi;
    x86reg edx;
    edx.e = mix_brrpos(voice);
    s2 s = mix_sample_interp(voice, esi, edi, edx);
    *(s4*)((u1*)DSPBuffer + esi * 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeR[voice]));
    *(s4*)((u1*)EchoBuffer + esi * 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeRe[voice]));
    *(s4*)((u1*)DSPBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeL[voice]));
    *(s4*)((u1*)EchoBuffer + esi * 4 + 4) += mix_scale(s, mix_vconv(voice, Voice0VolumeLe[voice]));
    MIX_TAIL_FREQ(voice, esi);
}

#endif /* DSP_MIXERS_H */
