/* Wide reads and writes at an arbitrary byte offset. The assembly did these
   with a plain mov, so the ported C inherited casts that are undefined when
   the address is not aligned and fault outright on a strict-alignment target.
   memcpy says the same thing legally; every compiler here folds it back into
   the single instruction the assembly used. */
#ifndef UNALIGNED_H
#define UNALIGNED_H

#include <stdint.h>
#include <string.h>

static inline uint32_t ld32u(void const* const p)
{
    uint32_t v;

    memcpy(&v, p, sizeof v);
    return v;
}

static inline void st32u(void* const p, uint32_t const v)
{
    memcpy(p, &v, sizeof v);
}

static inline uint16_t ld16u(void const* const p)
{
    uint16_t v;

    memcpy(&v, p, sizeof v);
    return v;
}

static inline void st16u(void* const p, uint16_t const v)
{
    memcpy(p, &v, sizeof v);
}

/* The same, but in the SNES's (and x86's) byte order regardless of the host.
   ld16u/ld32u read a host-order value - right for a host variable seen through
   a byte pointer, wrong for anything the emulated machine laid out: ROM, WRAM,
   VRAM, CGRAM, OAM, SPC RAM, and the x86 memory images the assembly assumed.
   Those go through these, which a little-endian compiler folds back into the
   plain load and a big-endian one (PowerPC) into a byte-reversed one. */
static inline uint16_t ld16le(void const* const p)
{
    uint8_t const* const b = (uint8_t const*)p;

    return (uint16_t)(b[0] | b[1] << 8);
}

static inline uint32_t ld32le(void const* const p)
{
    uint8_t const* const b = (uint8_t const*)p;

    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static inline void st16le(void* const p, uint16_t const v)
{
    uint8_t* const b = (uint8_t*)p;

    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
}

static inline void st32le(void* const p, uint32_t const v)
{
    uint8_t* const b = (uint8_t*)p;

    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
}

/* Two consecutive host halfwords - pixels, or entries of a table of words -
   loaded as one dword the way x86 did it: the first in the low half. */
static inline uint32_t ld16pair(void const* const p)
{
    return ld16u(p) | (uint32_t)ld16u((uint8_t const*)p + 2) << 16;
}

/* Host order is little-endian; true on x86, ARM and RISC-V Linux, false on
   PowerPC. Only for the few places that must pick a layout, not for loads. */
#ifndef ZSNES_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#define ZSNES_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN) || defined(__ARMEB__) || defined(__MIPSEB__)
/* gcc before 4.6 (Yellow Dog Linux ships 4.1-4.4) has no __BYTE_ORDER__, but
   always defines __BIG_ENDIAN__ on PowerPC. */
#define ZSNES_BIG_ENDIAN 1
#else
#define ZSNES_BIG_ENDIAN 0
#endif
#endif

/* Byte n of a host integer variable counting from its least significant end,
   as an lvalue - what the assembly meant by `[var+n]`. Refuses to compile when
   n is past the end of the variable: that would be reaching into whatever the
   x86 layout put next, which has to be written out explicitly instead. */
#define LE_BYTE(v, n)                            \
    (((uint8_t*)&(v))[((void)sizeof(struct { _Static_assert((n) < sizeof(v), "LE_BYTE past " #v); int x; }), \
        ZSNES_BIG_ENDIAN ? sizeof(v) - 1 - (n) : (n))])

/* The same with an index known only at run time; the caller keeps it in range. */
#define LE_BYTEV(v, n) (((uint8_t*)&(v))[ZSNES_BIG_ENDIAN ? sizeof(v) - 1 - (size_t)(n) : (size_t)(n)])

#endif
