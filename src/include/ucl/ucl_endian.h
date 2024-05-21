#ifndef __UCL_ENDIAN_H_INCLUDED
#define __UCL_ENDIAN_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

ucl_uint swap_uint16(ucl_uint val)
{
    return (val << 8) | (val >> 8);
}

ucl_int swap_int16(ucl_int val)
{
    return (val << 8) | ((val >> 8) & 0xFF);
}

ucl_uint32 swap_uint32(ucl_uint32 val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

ucl_int32 swap_int32(ucl_int32 val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | ((val >> 16) & 0xFFFF);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* already included */