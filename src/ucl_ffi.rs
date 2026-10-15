#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

use std::os::raw::*;

pub type ucl_uint = c_uint;
pub type ucl_uint32 = c_uint;

pub const UCL_E_OK: i32 = 0;
pub const UCL_E_OUT_OF_MEMORY: i32 = -2;
pub const UCL_VERSION: i32 = 0x0103;

#[repr(C)]
pub struct ucl_compress_config {
    pub bb_endian: c_int,
    pub bb_size: c_int,
    pub max_offset: ucl_uint,
    pub max_match: ucl_uint,
    pub s_level: c_int,
    pub h_level: c_int,
    pub p_level: c_int,
    pub c_flags: c_int,
    pub m_size: ucl_uint,
}

unsafe extern "C" {
    pub fn __ucl_init2(
        version: i32,
        s_size: i32,
        i_size: i32,
        l_size: i32,
        u32_size: i32,
        ui_size: i32,
        f_size: i32,
        p_size: i32,
        v_size: i32,
        fp_size: i32,
    ) -> c_int;

    pub fn ucl_nrv2b_decompress_le32(
        src: *const u8,
        src_len: ucl_uint,
        dst: *mut u8,
        dst_len: *mut ucl_uint,
        cb: *mut c_void,
    ) -> c_int;

    pub fn ucl_nrv2d_decompress_le32(
        src: *const u8,
        src_len: ucl_uint,
        dst: *mut u8,
        dst_len: *mut ucl_uint,
        cb: *mut c_void,
    ) -> c_int;

    pub fn ucl_nrv2e_decompress_le32(
        src: *const u8,
        src_len: ucl_uint,
        dst: *mut u8,
        dst_len: *mut ucl_uint,
        cb: *mut c_void,
    ) -> c_int;

    pub fn ucl_nrv2b_99_compress(
        src: *const u8,
        src_len: ucl_uint,
        dst: *mut u8,
        dst_len: *mut ucl_uint,
        wrkmem: *mut c_void,
        compression_level: c_int,
        cb: *const ucl_compress_config,
        progress: *mut c_void,
    ) -> c_int;

    pub fn ucl_nrv2d_99_compress(
        src: *const u8,
        src_len: ucl_uint,
        dst: *mut u8,
        dst_len: *mut ucl_uint,
        wrkmem: *mut c_void,
        compression_level: c_int,
        cb: *const ucl_compress_config,
        progress: *mut c_void,
    ) -> c_int;

    pub fn ucl_nrv2e_99_compress(
        src: *const u8,
        src_len: ucl_uint,
        dst: *mut u8,
        dst_len: *mut ucl_uint,
        wrkmem: *mut c_void,
        compression_level: c_int,
        cb: *const ucl_compress_config,
        progress: *mut c_void,
    ) -> c_int;

    pub fn ucl_adler32(adler: ucl_uint32, buf: *const u8, len: ucl_uint) -> ucl_uint32;
}

pub fn ucl_init() -> c_int {
    unsafe {
        __ucl_init2(
            UCL_VERSION,
            std::mem::size_of::<c_short>() as i32,
            std::mem::size_of::<c_int>() as i32,
            std::mem::size_of::<c_long>() as i32,
            std::mem::size_of::<u32>() as i32,
            std::mem::size_of::<c_uint>() as i32,
            -1i32,
            std::mem::size_of::<*mut u8>() as i32,
            std::mem::size_of::<*mut c_void>() as i32,
            std::mem::size_of::<*mut c_void>() as i32,
        )
    }
}
