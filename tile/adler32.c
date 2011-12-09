/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   TileGX implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#if defined(__GNUC__) && defined(__tilegx__)
#  define HAVE_ADLER32_VEC
#  define MIN_WORK 32
// TODO: VNMAX could prop. be a little higher?
#  define VNMAX (2*NMAX+((9*NMAX)/10))

#  define SOUL (sizeof(unsigned long))

/* ========================================================================= */
local inline unsigned long v1ddotpua(unsigned long d, unsigned long a, unsigned long b)
{
    __asm__ ("v1ddotpua	%0, %1, %2" : "=r" (d) : "r" (a), "r" (b), "0" (d));
    return d;
}

/* ========================================================================= */
local inline unsigned long v4shl(unsigned long a, unsigned long b)
{
    unsigned long r;
    __asm__ ("v4shl	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
    return r;
}

/* ========================================================================= */
local inline unsigned long v4shru(unsigned long a, unsigned long b)
{
    unsigned long r;
    __asm__ ("v4shru	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
    return r;
}

/* ========================================================================= */
local inline unsigned long v4sub(unsigned long a, unsigned long b)
{
    unsigned long r;
    __asm__ ("v4sub	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
    return r;
}

/* ========================================================================= */
local inline unsigned long v4add(unsigned long a, unsigned long b)
{
    unsigned long r;
    __asm__ ("v4add	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
    return r;
}

/* ========================================================================= */
local inline unsigned long vector_chop(unsigned long x)
{
    unsigned long y;

    y = v4shl(x, 16);
    y = v4shru(y, 16);
    x = v4shru(x, 16);
    y = v4sub(y, x);
    x = v4shl(x, 4);
    x = v4add(x, y);
    return x;
}

/* ========================================================================= */
local noinline uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned int s1, s2;
    unsigned k;

    /* split Adler-32 into component sums */
    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    /* align input */
    k    = ALIGN_DIFF(buf, SOUL);
    len -= k;
    if (k) do {
        s1 += *buf++;
        s2 += s1;
    } while (--k);

    k = len < VNMAX ? len : VNMAX;
    len -= k;
    if (likely(k >= 2 * SOUL)) {
        const unsigned long v1 = 0x0101010101010101ul;
        unsigned long vs1 = s1, vs2 = s2;
        unsigned long vorder;

        if(host_is_bigendian())
            vorder = 0x0807060504030201;
        else
            vorder = 0x0102030405060708;

        do {
            unsigned long vs1_r = 0;
            do {
                /* get input data */
                unsigned long in = *(const unsigned long *)buf;
                /* add vs1 for this round */
                vs1_r += vs1; /* we don't overflow, so normal add */
                /* add horizontal & acc vs1 */
                vs1 = v1ddotpua(vs1, in, v1); /* only into x0 pipeline */
                /* mul, add & acc vs2 */
                vs2 = v1ddotpua(vs2, in, vorder); /* only in x0 pipe */
                buf += SOUL;
                k -= SOUL;
            } while (k >= SOUL);
            /* chop vs1 round sum before multiplying by 8 */
            vs1_r = vector_chop(vs1_r);
            /* add vs1 for this round (8 times) */
            vs1_r = v4shl(vs1_r, 3);
            vs2 += vs1_r;
            /* chop both sums */
            vs2 = vector_chop(vs2);
            vs1 = vector_chop(vs1);
            len += k;
            k = len < VNMAX ? len : VNMAX;
            len -= k;
        } while (likely(k >= SOUL));
        s1 = (vs1 & 0xffffffff) + (vs1 >> 32);
        s2 = (vs2 & 0xffffffff) + (vs2 >> 32);
    }

    /* handle trailer */
    if (unlikely(k)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--k);
    /* at this point s1 & s2 should be in range */
    MOD28(s1);
    MOD28(s2);

    /* return recombined sums */
    return (s2 << 16) | s1;
}
#endif