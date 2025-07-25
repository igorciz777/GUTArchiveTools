#ifndef __GUT_FILE_H_INCLUDED
#define __GUT_FILE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* fwrite wrapper */
uint32_t xwrite(FILE *f, const void* buf, uint32_t len)
{
    uint32_t l;

    if (f != NULL)
    {
        l = (uint32_t)fwrite(buf, 1, len, f);
        if (l != len)
        {
            fprintf(stderr, "\nwrite error [%ld %ld]\n",
                    (long)len, (long)l);
            return 0;
        }
    }
    return len;
}

/* fread wrapper */
uint32_t xread(FILE *f, void* buf, uint32_t len, bool allow_eof)
{
    uint32_t l;
    l = (uint32_t)fread(buf, 1, len, f);
    if (l > len)
    {
        fprintf(stderr, "\nC library read error\n");
        return 0;
    }
    if (l != len && !allow_eof)
    {
        fprintf(stderr, "\nread error - premature end of file\n");
        return 0;
    }
    return l;
}

/* Read 32-bit integer in big-endian format */
uint32_t xread32(FILE *f)
{
    unsigned char b[4];
    uint32_t v;

    xread(f, b, 4, 0);
    v = (uint32_t)b[3] << 0;
    v |= (uint32_t)b[2] << 8;
    v |= (uint32_t)b[1] << 16;
    v |= (uint32_t)b[0] << 24;
    return v;
}

/* Write 32-bit integer in big-endian format */
void xwrite32(FILE *f, uint32_t v)
{
    unsigned char b[4];

    b[3] = (unsigned char)(v >> 0);
    b[2] = (unsigned char)(v >> 8);
    b[1] = (unsigned char)(v >> 16);
    b[0] = (unsigned char)(v >> 24);
    xwrite(f, b, 4);
}

/* Read a single byte */
int xgetc(FILE *f)
{
    unsigned char c;
    xread(f, (void*)&c, 1, 0);
    return c;
}

/* Write a single byte */
void xputc(FILE *f, int c)
{
    unsigned char cc = (unsigned char)c;
    xwrite(f, (const void*)&cc, 1);
}

/* Swap 32-bit unsigned integer endianness */
uint32_t swap_uint32(uint32_t val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

/* Swap 32-bit signed integer endianness */
int32_t swap_int32(int32_t val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | ((val >> 16) & 0xFFFF);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __GUT_FILE_H_INCLUDED */