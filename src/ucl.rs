use std::io::{Read, Seek, Write};
use std::ptr;

use crate::file_io::{xgetc, xputc, xread, xread32be, xwrite, xwrite32be};
use crate::headers::UCL_MAGIC;
use crate::ucl_ffi::*;

fn get_overhead(method: u8, size: ucl_uint) -> ucl_uint {
    if method == 0x2b || method == 0x2d || method == 0x2e {
        size / 8 + 256
    } else {
        0
    }
}

fn set_method_name(method: u8, level: u8) -> bool {
    level >= 1 && level <= 10 && matches!(method, 0x2b | 0x2d | 0x2e)
}

pub fn do_decompress(fi: &mut (impl Read + Seek), fo: &mut impl Write) -> Result<(), String> {
    let mut m = [0u8; 8];
    if xread(fi, &mut m, true).map_err(|e| e.to_string())? != 8 || m != *UCL_MAGIC {
        return Err(
            "header error - this file is not compressed by uclpack\n\
             check compatibility mode"
                .into(),
        );
    }

    let _flags = xread32be(fi).map_err(|e| e.to_string())?;
    let method = xgetc(fi).map_err(|e| e.to_string())?;
    let level = xgetc(fi).map_err(|e| e.to_string())?;
    let block_size = xread32be(fi).map_err(|e| e.to_string())?;

    let overhead = get_overhead(method, block_size);
    if overhead == 0 || !set_method_name(method, level) {
        return Err(format!(
            "header error - invalid method {} (level {})",
            method, level
        ));
    }
    if block_size < 32 || block_size > 32 * 1024 * 1024 {
        return Err(format!(
            "header error - invalid block size {}",
            block_size
        ));
    }

    let _checksum = unsafe { ucl_adler32(0, ptr::null(), 0) };

    let buf_len = (block_size + overhead) as usize;
    let mut buf = vec![0u8; buf_len];

    loop {
        let out_len = xread32be(fi).map_err(|e| e.to_string())?;
        if out_len == 0 {
            break;
        }

        let in_len = xread32be(fi).map_err(|e| e.to_string())?;

        if in_len > block_size || out_len > block_size || in_len == 0 || in_len > out_len {
            return Err("block size error - data corrupted".into());
        }

        let in_start = buf_len - in_len as usize;
        let (_, in_slice) = buf.split_at_mut(in_start);
        xread(fi, &mut in_slice[..in_len as usize], false).map_err(|e| e.to_string())?;

        if in_len < out_len {
            let mut new_len = out_len;
            let r = unsafe {
                match method {
                    0x2b => ucl_nrv2b_decompress_le32(
                        buf.as_ptr().add(in_start),
                        in_len,
                        buf.as_mut_ptr(),
                        &mut new_len,
                        ptr::null_mut(),
                    ),
                    0x2d => ucl_nrv2d_decompress_le32(
                        buf.as_ptr().add(in_start),
                        in_len,
                        buf.as_mut_ptr(),
                        &mut new_len,
                        ptr::null_mut(),
                    ),
                    0x2e => ucl_nrv2e_decompress_le32(
                        buf.as_ptr().add(in_start),
                        in_len,
                        buf.as_mut_ptr(),
                        &mut new_len,
                        ptr::null_mut(),
                    ),
                    _ => return Err(format!("unknown method {}", method)),
                }
            };
            if r != UCL_E_OK as i32 || new_len != out_len {
                return Err(format!(
                    "compressed data violation: error {} (0x{:x}: {}/{}/{})",
                    r, method, in_len, out_len, new_len
                ));
            }
            xwrite(fo, &buf[..out_len as usize]).map_err(|e| e.to_string())?;
        } else {
            xwrite(fo, &in_slice[..in_len as usize]).map_err(|e| e.to_string())?;
        }
    }

    if _flags & 1 != 0 {
        xread32be(fi).map_err(|e| e.to_string())?;
    }

    Ok(())
}

pub fn do_compress(
    fi: &mut impl Read,
    fo: &mut impl Write,
    method: u8,
    level: u8,
    block_size: ucl_uint,
) -> Result<(), String> {
    let _flags: u32 = 0;
    let overhead = get_overhead(method, block_size);

    xwrite(fo, UCL_MAGIC).map_err(|e| e.to_string())?;
    xwrite32be(fo, _flags).map_err(|e| e.to_string())?;
    xputc(fo, method).map_err(|e| e.to_string())?;
    xputc(fo, level).map_err(|e| e.to_string())?;
    xwrite32be(fo, block_size).map_err(|e| e.to_string())?;

    let out_capacity = (block_size + overhead) as usize;
    let mut in_buf = vec![0u8; block_size as usize];
    let mut out_buf = vec![0u8; out_capacity];

    let cfg = ucl_compress_config {
        bb_endian: 0,
        bb_size: 32,
        max_offset: !0,
        max_match: !0,
        s_level: -1,
        h_level: -1,
        p_level: -1,
        c_flags: 0,
        m_size: !0,
    };

    loop {
        let in_len = xread(fi, &mut in_buf, true).map_err(|e| e.to_string())?;
        if in_len == 0 {
            break;
        }

        let mut out_len: ucl_uint = 0;

        let r = unsafe {
            match method {
                0x2b => ucl_nrv2b_99_compress(
                    in_buf.as_ptr(),
                    in_len as ucl_uint,
                    out_buf.as_mut_ptr(),
                    &mut out_len,
                    ptr::null_mut(),
                    level as i32,
                    &cfg,
                    ptr::null_mut(),
                ),
                0x2d => ucl_nrv2d_99_compress(
                    in_buf.as_ptr(),
                    in_len as ucl_uint,
                    out_buf.as_mut_ptr(),
                    &mut out_len,
                    ptr::null_mut(),
                    level as i32,
                    &cfg,
                    ptr::null_mut(),
                ),
                0x2e => ucl_nrv2e_99_compress(
                    in_buf.as_ptr(),
                    in_len as ucl_uint,
                    out_buf.as_mut_ptr(),
                    &mut out_len,
                    ptr::null_mut(),
                    level as i32,
                    &cfg,
                    ptr::null_mut(),
                ),
                _ => return Err(format!("unknown method {}", method)),
            }
        };

        if r == UCL_E_OUT_OF_MEMORY as i32 {
            return Err("out of memory in compress".into());
        }
        if r != UCL_E_OK as i32
            || out_len > in_len as ucl_uint + get_overhead(method, in_len as ucl_uint)
        {
            return Err(format!("internal error - compression failed: {}", r));
        }

        xwrite32be(fo, in_len as ucl_uint).map_err(|e| e.to_string())?;

        if out_len < in_len as ucl_uint {
            xwrite32be(fo, out_len).map_err(|e| e.to_string())?;
            xwrite(fo, &out_buf[..out_len as usize]).map_err(|e| e.to_string())?;
        } else {
            xwrite32be(fo, in_len as ucl_uint).map_err(|e| e.to_string())?;
            xwrite(fo, &in_buf[..in_len]).map_err(|e| e.to_string())?;
        }
    }

    xwrite32be(fo, 0).map_err(|e| e.to_string())?;

    Ok(())
}
