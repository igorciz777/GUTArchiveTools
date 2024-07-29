#ifndef __UCL_FUNC_H_INCLUDED
#define __UCL_FUNC_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

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

ucl_uint xwrite(FILE *f, const ucl_voidp buf, ucl_uint len)
{
    ucl_uint l;

    if (f != NULL)
    {
        l = (ucl_uint)ucl_fwrite(f, buf, len);
        if (l != len)
        {
            fprintf(stderr, "\nwrite error [%ld %ld]  (disk full ?)\n",
                    (long)len, (long)l);
            exit(1);
        }
    }
    return len;
}

ucl_uint xread(FILE *f, ucl_voidp buf, ucl_uint len, ucl_bool allow_eof)
{
    ucl_uint l;

    l = (ucl_uint)ucl_fread(f, buf, len);
    if (l > len)
    {
        fprintf(stderr, "\nsomething's wrong with your C library !!!\n");
        exit(1);
    }
    if (l != len && !allow_eof)
    {
        fprintf(stderr, "\nread error - premature end of file, check compatibility mode\n");
        exit(1);
    }
    return l;
}

ucl_uint32 xread32(FILE *f)
{
    unsigned char b[4];
    ucl_uint32 v;

    xread(f, b, 4, 0);
    v = (ucl_uint32)b[3] << 0;
    v |= (ucl_uint32)b[2] << 8;
    v |= (ucl_uint32)b[1] << 16;
    v |= (ucl_uint32)b[0] << 24;
    return v;
}

void xwrite32(FILE *f, ucl_uint32 v)
{
    unsigned char b[4];

    b[3] = (unsigned char)(v >> 0);
    b[2] = (unsigned char)(v >> 8);
    b[1] = (unsigned char)(v >> 16);
    b[0] = (unsigned char)(v >> 24);
    xwrite(f, b, 4);
}

int xgetc(FILE *f)
{
    unsigned char c;
    xread(f, (ucl_voidp)&c, 1, 0);
    return c;
}

void xputc(FILE *f, int c)
{
    unsigned char cc = (unsigned char)c;
    xwrite(f, (const ucl_voidp)&cc, 1);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* already included */