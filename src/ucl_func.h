#ifndef __UCL_FUNC_H_INCLUDED
#define __UCL_FUNC_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <ucl/ucl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "gut_headers.h"
#include "gut_file.h"

static char method_name[64];

static ucl_uint get_overhead(int method, ucl_uint size)
{
    if (method == 0x2b || method == 0x2d || method == 0x2e)
        return size / 8 + 256;
    return 0;
}

static ucl_bool set_method_name(int method, int level)
{
    method_name[0] = 0;
    if (level < 1 || level > 10)
        return 0;
    if (method == 0x2b)
        sprintf(method_name, "NRV2B-99/%d", level);
    else if (method == 0x2d)
        sprintf(method_name, "NRV2D-99/%d", level);
    else if (method == 0x2e)
        sprintf(method_name, "NRV2E-99/%d", level);
    else
        return 0;
    return 1;
}

int do_decompress(FILE *fi, FILE *fo)
{
    int result;
    int method;
    int level;
    ucl_uint buf_len;
    ucl_uint block_size;
    ucl_uint overhead = 0;
    ucl_uint32 checksum;
    ucl_uint32 flags;
    ucl_uint32 opt_fast = 1;
    ucl_bytep buf = NULL;
    unsigned char m[sizeof(UCL_MAGIC)];

    result = 0;

    /*
     * Step 1: check UCL_MAGIC header, read flags & block size, init checksum
     */
    if (xread(fi, m, sizeof(UCL_MAGIC), 1) != sizeof(UCL_MAGIC) ||
        memcmp(m, UCL_MAGIC, sizeof(UCL_MAGIC)) != 0)
    {
        printf("header error - this file is not compressed by uclpack\n");
        printf("check compatibility mode\n");
        result = 1;
        goto err;
    }
    flags = xread32(fi);
    method = xgetc(fi);
    level = xgetc(fi);
    block_size = xread32(fi);
    overhead = get_overhead(method, block_size);
    if (overhead == 0 || !set_method_name(method, level))
    {
        printf("header error - invalid method %d (level %d)\n", method, level);
        result = 2;
        goto err;
    }
    if (block_size < 32 || block_size > 32 * 1024 * 1024L)
    {
        printf("header error - invalid block size %ld\n", (long)block_size);
        result = 3;
        goto err;
    }

    checksum = ucl_adler32(0, NULL, 0);

    /*
     * Step 2: allocate buffer for in-place decompression
     */
    buf_len = block_size + overhead;
    buf = (ucl_bytep)ucl_malloc(buf_len);
    if (buf == NULL)
    {
        printf("out of memory\n");
        result = 4;
        goto err;
    }

    /*
     * Step 3: process blocks
     */
    for (;;)
    {
        ucl_bytep in;
        ucl_bytep out;
        ucl_uint in_len;
        ucl_uint out_len;

        /* read uncompressed size */
        out_len = xread32(fi);

        /* exit if last block (EOF marker) */
        if (out_len == 0)
            break;

        /* read compressed size */
        in_len = xread32(fi);

        /* sanity check of the size values */
        if (in_len > block_size || out_len > block_size ||
            in_len == 0 || in_len > out_len)
        {
            printf("block size error - data corrupted\n");
            result = 5;
            goto err;
        }

        /* place compressed block at the top of the buffer */
        in = buf + buf_len - in_len;
        out = buf;

        /* read compressed block data */
        xread(fi, in, in_len, 0);

        if (in_len < out_len)
        {
            /* decompress - use safe decompressor as data might be corrupted */
            ucl_uint new_len = out_len;

            if (method == 0x2b)
            {
                result = ucl_nrv2b_decompress_le32(in, in_len, out, &new_len, NULL);
            }
            else if (method == 0x2d)
            {
                result = ucl_nrv2d_decompress_le32(in, in_len, out, &new_len, NULL);
            }
            else if (method == 0x2e)
            {
                result = ucl_nrv2e_decompress_le32(in, in_len, out, &new_len, NULL);
            }
            if (result != UCL_E_OK || new_len != out_len)
            {
                printf("compressed data violation: error %d (0x%x: %ld/%ld/%ld)\n", result, method, (long)in_len, (long)out_len, (long)new_len);
                result = 6;
                goto err;
            }

            /* write decompressed block */
            xwrite(fo, out, out_len);
        }
        else
        {
            /* write original (incompressible) block */
            xwrite(fo, in, in_len);
        }
    }

    /* read and verify checksum */
    if (flags & 1)
    {
        ucl_uint32 c = xread32(fi);
        if (!opt_fast && c != checksum)
        {
            printf("checksum error - data corrupted\n");
            result = 7;
            goto err;
        }
    }

    result = 0;
err:
    ucl_free(buf);
    return result;
}

int do_compress(FILE *fi, FILE *fo, int method, int level, ucl_uint block_size)
{
    int r = 0;
    ucl_bytep in = NULL;
    ucl_bytep out = NULL;
    ucl_uint in_len;
    ucl_uint out_len;
    ucl_uint32 flags = 1; //opt_fast == true
    ucl_uint32 checksum;
    ucl_uint overhead = 0;

    struct ucl_compress_config_t cfg;

    /* little endian 32 config*/
    memset(&cfg, 0xff, sizeof(cfg));
    cfg.bb_endian = 0;
    cfg.bb_size = 32;
    cfg.c_flags = 0;

    /*
     * Step 1: write UCL_MAGIC header, flags & block size, init checksum
     */
    xwrite(fo, UCL_MAGIC, sizeof(UCL_MAGIC));
    xwrite32(fo, flags);
    xputc(fo, method); /* compression method */
    xputc(fo, level);  /* compression level */
    xwrite32(fo, block_size);
    checksum = ucl_adler32(0, NULL, 0);

    /*
     * Step 2: allocate compression buffers and work-memory
     */
    overhead = get_overhead(method, block_size);
    in = (ucl_bytep)ucl_malloc(block_size);
    out = (ucl_bytep)ucl_malloc(block_size + overhead);
    if (in == NULL || out == NULL)
    {
        printf("out of memory\n");
        r = 1;
        goto err;
    }

    /*
     * Step 3: process blocks
     */
    for (;;)
    {
        /* read block */
        in_len = xread(fi, in, block_size, 1);
        if (in_len <= 0)
            break;

        /* update checksum */
        if (flags & 1)
            checksum = ucl_adler32(checksum, in, in_len);

        /* compress block */
        r = UCL_E_ERROR;
        out_len = 0;
        if (method == 0x2b)
            r = ucl_nrv2b_99_compress(in, in_len, out, &out_len, 0, level, &cfg, NULL);
        else if (method == 0x2d)
            r = ucl_nrv2d_99_compress(in, in_len, out, &out_len, 0, level, &cfg, NULL);
        else if (method == 0x2e)
            r = ucl_nrv2e_99_compress(in, in_len, out, &out_len, 0, level, &cfg, NULL);
        if (r == UCL_E_OUT_OF_MEMORY)
        {
            printf("out of memory in compress\n");
            r = 1;
            goto err;
        }
        if (r != UCL_E_OK || out_len > in_len + get_overhead(method, in_len))
        {
            /* this should NEVER happen */
            printf("internal error - compression failed: %d\n", r);
            r = 2;
            goto err;
        }
        /* write uncompressed block size */
        xwrite32(fo, in_len);

        if (out_len < in_len)
        {
            /* write compressed block */
            xwrite32(fo, out_len);
            xwrite(fo, out, out_len);
        }
        else
        {
            /* not compressible - write uncompressed block */
            xwrite32(fo, in_len);
            xwrite(fo, in, in_len);
        }
    }

    /* write EOF marker */
    xwrite32(fo, 0);

    /* write checksum */
    if (flags & 1)
        xwrite32(fo, checksum);

    r = 0;
err:
    ucl_free(out);
    ucl_free(in);
    return r;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* already included */