use std::io::{self, Read, Write};

/// Reads exactly `len` bytes from `reader`. Returns `Ok(n)` on success
/// where `n == len`. If `allow_eof`, a short read is acceptable.
pub fn xread(reader: &mut impl Read, buf: &mut [u8], allow_eof: bool) -> io::Result<usize> {
    if buf.is_empty() {
        return Ok(0);
    }
    let n = reader.read(buf)?;
    if !allow_eof && n != buf.len() {
        return Err(io::Error::new(io::ErrorKind::UnexpectedEof, "premature end of file"));
    }
    Ok(n)
}

/// Writes all bytes to `writer`.
pub fn xwrite(writer: &mut impl Write, buf: &[u8]) -> io::Result<()> {
    writer.write_all(buf)
}

/// Reads a big-endian u32 (used for UCL headers).
pub fn xread32be(reader: &mut impl Read) -> io::Result<u32> {
    let mut b = [0u8; 4];
    xread(reader, &mut b, false)?;
    Ok(u32::from_be_bytes(b))
}

/// Writes a u32 in big-endian byte order (used for UCL headers).
pub fn xwrite32be(writer: &mut impl Write, v: u32) -> io::Result<()> {
    writer.write_all(&v.to_be_bytes())
}

/// Reads a little-endian u32 (used for TOC and .dat headers).
pub fn xread32le(reader: &mut impl Read) -> io::Result<u32> {
    let mut b = [0u8; 4];
    xread(reader, &mut b, false)?;
    Ok(u32::from_le_bytes(b))
}

/// Writes a u32 in little-endian byte order.
#[allow(dead_code)]
pub fn xwrite32le(writer: &mut impl Write, v: u32) -> io::Result<()> {
    writer.write_all(&v.to_le_bytes())
}

/// Reads a single byte.
pub fn xgetc(reader: &mut impl Read) -> io::Result<u8> {
    let mut b = [0u8; 1];
    xread(reader, &mut b, false)?;
    Ok(b[0])
}

/// Writes a single byte.
pub fn xputc(writer: &mut impl Write, c: u8) -> io::Result<()> {
    writer.write_all(&[c])
}

/// Swaps the byte order of a u32.
pub fn swap_uint32(val: u32) -> u32 {
    val.swap_bytes()
}


