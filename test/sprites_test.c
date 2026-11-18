// Regression tests for the C sprite cache port (video/sprites.c).
//
// processspritesb and cachesprites were ported from vcache.asm and verified
// byte-for-byte against the original assembly with a differential harness.
// That harness needs the asm reference, which no longer exists, so this test
// instead pins the ported output with golden checksums over a deterministic
// set of randomized inputs.  The checksums are address independent: the sprite
// table stores absolute char addresses, so processspritesb runs with a fixed
// sentinel vcache4b base (it never dereferences it), while cachesprites runs
// against a real buffer.

#include <stdint.h>
#include <string.h>

#include "../unaligned.h"
#include "../video/makevid.h"
#include "zstest.h"

typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;

// Globals the port references (defined here for the standalone test, in
// cpu/regs.inc and endmem.asm for the real build).
u1 oamram[1024];
u1 objsize1, objsize2, objhipr, interlval;
u4 objptr, objptrn;
u2 objadds1, objadds2;
u1 objmovs1, objmovs2;
u2 resolutn, curypos;
u1 sprlefttot[256], sprleftpr[1024], sprcnt[256], sprstart[256];
u1 sprtilecnt[256], sprend[256];
u2 sprendx[256];
static u1 spt[256 * 512];
u1* spritetablea = spt;
static u1 vrambuf[65536];
u1* vram = vrambuf;
static u1 vc4[131072 + 256];
u1* vcache4b;
u2 vidmemch4[2048];
u1 tltype4b[2048];
u1 sprprifix = 1;
u4 addr2add;

void processsprites(void);
void cachesprites(void);

static u4 rng;
static u4 rnd(void)
{
    rng = rng * 1664525u + 1013904223u;
    return rng;
}

static u4 mix(u4 h, const void* p, int n)
{
    const u1* b = p;
    for (int i = 0; i < n; i++)
        h = h * 16777619u ^ b[i];
    return h;
}

/* The goldens were taken on x86, hashing these tables' bytes as they sit in
   memory. Hash the multi-byte fields low byte first, so a big-endian host
   computes the same numbers from the same values. */
static u4 mix_le(u4 h, uint64_t const v, int const n)
{
    for (int i = 0; i < n; i++)
        h = h * 16777619u ^ (u1)(v >> (8 * i));
    return h;
}

static u4 mix_u2s(u4 h, u2 const* const p, int const n)
{
    for (int i = 0; i < n; i++)
        h = mix_le(h, p[i], 2);
    return h;
}

static u4 mix_spt(u4 h)
{
    size_t i;

    for (i = 0; i + sizeof(SpriteInfo) <= sizeof spt; i += sizeof(SpriteInfo)) {
        SpriteInfo r;
        memcpy(&r, spt + i, sizeof r);
        h = mix_le(h, r.x, 2);
        h = mix_le(h, (uint64_t)(uintptr_t)r.obj, (int)sizeof r.obj);
        h = mix_le(h, r.pal, 1);
        h = mix_le(h, r.status, 1);
    }
    /* The table is not a whole number of 12-byte records on a 64-bit build. */
    return mix(h, spt + i, (int)(sizeof spt - i));
}

static void setup(int t)
{
    static const u1 sz[6] = { 1, 4, 16, 64, 2, 3 };
    static const u2 rs[4] = { 224, 239, 256, 255 };
    rng = 0x1234 + t * 2654435761u;
    for (int i = 0; i < 1024; i++)
        oamram[i] = rnd();
    objsize1 = sz[rnd() % 6];
    objsize2 = sz[rnd() % 6];
    objhipr = rnd() & 0x7f;
    interlval = rnd() & 0xff;
    objptr = rnd() & 0x7fff;
    objptrn = rnd() & 0x7fff;
    objadds1 = rnd() & 0xffff;
    objadds2 = rnd() & 0xffff;
    objmovs1 = rnd() & 0xff;
    objmovs2 = rnd() & 0xff;
    resolutn = rs[rnd() % 4];
    curypos = rnd() % resolutn;
    for (int i = 0; i < 65536; i++)
        vrambuf[i] = rnd();
    for (int i = 0; i < 2048; i++)
        st16le(&vidmemch4[i], (u2)rnd()); /* read back a byte at a time */
    sprprifix = 1;
    memset(sprlefttot, 0, 256);
    memset(sprleftpr, 0, 1024);
    memset(sprcnt, 0, 256);
    memset(sprstart, 0, 256);
    memset(sprtilecnt, 0, 256);
    memset(sprend, 0, 256);
    memset(sprendx, 0, sizeof sprendx);
    memset(spt, 0, sizeof spt);
    memset(vc4, 0, sizeof vc4);
    memset(tltype4b, 0, 2048);
    addr2add = 0;
}

int main(void)
{
    // Golden checksums captured from the asm-verified port. One pair per
    // pointer size: spritetablea holds SpriteInfo records with a host pointer
    // in them, so the stride, and every byte hashed out of the table, depends
    // on it. The 64-bit pair is the same on x86-64 and aarch64.
#if UINTPTR_MAX > 0xffffffffu
    const u4 PROC_GOLDEN = 0xad7f9bbdu;
    const u4 CACHE_GOLDEN = 0xd8237b01u;
#else
    const u4 PROC_GOLDEN = 0x99cfdaf2u;
    const u4 CACHE_GOLDEN = 0x4b5fb5bcu;
#endif

    u4 hp = 2166136261u, hc = 2166136261u;
    for (int t = 0; t < 64; t++) {
        setup(t);
        // Sentinel base: processspritesb only stores vcache4b+offset, never reads it.
        vcache4b = (u1*)0x10000000u;
        processsprites();
        hp = mix(hp, sprlefttot, 256);
        hp = mix(hp, sprleftpr, 1024);
        hp = mix(hp, sprcnt, 256);
        hp = mix(hp, sprstart, 256);
        hp = mix(hp, sprtilecnt, 256);
        hp = mix(hp, sprend, 256);
        hp = mix_u2s(hp, sprendx, 256);
        hp = mix_spt(hp);
        hp = mix_le(hp, addr2add, 4);
        vcache4b = vc4;
        cachesprites();
        hc = mix(hc, vc4, sizeof vc4);
        hc = mix(hc, tltype4b, 2048);
    }

    printf("sprite cache port tests\n");
    ZT_SECTION("processspritesb: sprite table + per-line limits (64 seeds)");
    ZT_CHECK_INT(hp, PROC_GOLDEN);
    ZT_SECTION("cachesprites: 4bpp sprite tile decode (64 seeds)");
    ZT_CHECK_INT(hc, CACHE_GOLDEN);
    ZT_RESULTS();
}
