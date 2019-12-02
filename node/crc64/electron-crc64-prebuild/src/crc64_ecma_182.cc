#include "crc64_ecma_182.h"

namespace crc64js_base {

#define POLY UINT64_C(0xc96c5795d7870f42)

/**
 * Tables for CRC calculation -- filled in by initialization functions that are
 * called once.  These could be replaced by constant tables generated in the
 * same way.  There are two tables, one for each endianess.  Since these are
 * static, i.e. local, one should be compiled out of existence if the compiler
 * can evaluate the endianess check in crc64() at compile time.
 */
static uint64_t crc64_little_table[8][256];
static uint64_t crc64_big_table[8][256];

/* Reverse the bytes in a 64-bit word. */
static inline uint64_t rev8(uint64_t a)
{
    uint64_t m;

    m = UINT64_C(0xff00ff00ff00ff);
    a = ((a >> 8) & m) | (a & m) << 8;
    m = UINT64_C(0xffff0000ffff);
    a = ((a >> 16) & m) | (a & m) << 16;
    return a >> 32 | a << 32;
}

/* Fill in the CRC-64 constants table. */
static void crc64_init_(uint64_t table[][256])
{
    unsigned n, k;
    uint64_t crc;

    /* generate CRC-64's for all single byte sequences */
    for(n = 0; n < 256; n++) {
        crc = n;
        for(k = 0; k < 8; k++)
            crc = crc & 1 ? POLY ^ (crc >> 1) : crc >> 1;
        table[0][n] = crc;
    }

    /* generate CRC-64's for those followed by 1 to 7 zeros */
    for(n = 0; n < 256; n++) {
        crc = table[0][n];
        for(k = 1; k < 8; k++) {
            crc = table[0][crc & 0xff] ^ (crc >> 8);
            table[k][n] = crc;
        }
    }
}

/**
 * This function is called once to initialize the CRC-64 table for use on a
 * little-endian architecture.
 */
static void crc64_little_init()
{
    crc64_init_(crc64_little_table);
}

/**
 * This function is called once to initialize the CRC-64 table for use on a
 * big-endian architecture.
 */
static void crc64_big_init()
{
    unsigned k, n;

    crc64_init_(crc64_big_table);
    for(k = 0; k < 8; k++)
        for(n = 0; n < 256; n++)
            crc64_big_table[k][n] = rev8(crc64_big_table[k][n]);
}

void crc64_init()
{
    uint64_t n = 1;
    *(char*)&n ? crc64_little_init() : crc64_big_init();
}

/* Calculate a CRC-64 eight bytes at a time on a little-endian architecture. */
static inline uint64_t crc64_little(uint64_t crc, void *buf, size_t len)
{
    unsigned char *next = static_cast<unsigned char*>(buf);

    crc = ~crc;
    while(len && ((uintptr_t)next & 7) != 0) {
        crc = crc64_little_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    while(len >= 8) {
        crc ^= *(uint64_t *)next;
        crc = crc64_little_table[7][crc & 0xff] ^
              crc64_little_table[6][(crc >> 8) & 0xff] ^
              crc64_little_table[5][(crc >> 16) & 0xff] ^
              crc64_little_table[4][(crc >> 24) & 0xff] ^
              crc64_little_table[3][(crc >> 32) & 0xff] ^
              crc64_little_table[2][(crc >> 40) & 0xff] ^
              crc64_little_table[1][(crc >> 48) & 0xff] ^
              crc64_little_table[0][crc >> 56];
        next += 8;
        len -= 8;
    }
    while(len) {
        crc = crc64_little_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    return ~crc;
}

/* Calculate a CRC-64 eight bytes at a time on a big-endian architecture. */
static inline uint64_t crc64_big(uint64_t crc, void *buf, size_t len)
{
    unsigned char *next = static_cast<unsigned char*>(buf);

    crc = ~rev8(crc);
    while(len && ((uintptr_t)next & 7) != 0) {
        crc = crc64_big_table[0][(crc >> 56) ^ *next++] ^ (crc << 8);
        len--;
    }
    while(len >= 8) {
        crc ^= *(uint64_t *)next;
        crc = crc64_big_table[0][crc & 0xff] ^
              crc64_big_table[1][(crc >> 8) & 0xff] ^
              crc64_big_table[2][(crc >> 16) & 0xff] ^
              crc64_big_table[3][(crc >> 24) & 0xff] ^
              crc64_big_table[4][(crc >> 32) & 0xff] ^
              crc64_big_table[5][(crc >> 40) & 0xff] ^
              crc64_big_table[6][(crc >> 48) & 0xff] ^
              crc64_big_table[7][crc >> 56];
        next += 8;
        len -= 8;
    }
    while(len) {
        crc = crc64_big_table[0][(crc >> 56) ^ *next++] ^ (crc << 8);
        len--;
    }
    return ~rev8(crc);
}

uint64_t crc64(uint64_t crc, void *buf, size_t len)
{
    uint64_t n = 1;

    return *(char *)&n ? crc64_little(crc, buf, len) :
                         crc64_big(crc, buf, len);
}


#define GF2_DIM 64      /* dimension of GF(2) vectors (length of CRC) */

static uint64_t gf2_matrix_times(uint64_t *mat, uint64_t vec)
{
    uint64_t sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

static void gf2_matrix_square(uint64_t *square, uint64_t *mat)
{
    unsigned n;

    for (n = 0; n < GF2_DIM; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

/* Return the CRC-64 of two sequential blocks, where crc1 is the CRC-64 of the
 first block, crc2 is the CRC-64 of the second block, and len2 is the length
 of the second block. */
uint64_t crc64_combine(uint64_t crc1, uint64_t crc2, uintmax_t len2)
{
    unsigned n;
    uint64_t row;
    uint64_t even[GF2_DIM];     /* even-power-of-two zeros operator */
    uint64_t odd[GF2_DIM];      /* odd-power-of-two zeros operator */

    /* degenerate case */
    if (len2 == 0)
        return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = POLY;              /* CRC-64 polynomial */
    row = 1;
    for (n = 1; n < GF2_DIM; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
     zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}

}
