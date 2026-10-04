use std::fs;
use std::io::{Cursor, Read, Seek, SeekFrom};
use std::path::Path;

use crate::headers::{find_file_extension_footer, find_file_extension_header};
use crate::log::{LogType, log_printf};


fn parse_dat_offsets(buffer: &[u8]) -> Option<Vec<usize>> {
    if buffer.len() < 0x8 {
        return None;
    }

    let file_size = buffer.len();
    let file_count = u32::from_le_bytes([buffer[0], buffer[1], buffer[2], buffer[3]]) as usize;

    if file_count == 0 || file_count > (file_size / 4) + 4 {
        return None;
    }

    // header must fit inside the file
    if 4 + file_count * 4 > file_size {
        return None;
    }

    let mut offsets = Vec::with_capacity(file_count + 1);
    for i in 0..file_count {
        let p = 4 + i * 4;
        let offset = u32::from_le_bytes([buffer[p], buffer[p + 1], buffer[p + 2], buffer[p + 3]]);
        if offset == 0 || offset as usize >= file_size || offset == 0xFFFF_FFFF {
            return None;
        }
        // Bakumatsuden edge case
        if file_count == 0x0F && offset == 0x29 && i < 1 {
            return None;
        }
        offsets.push(offset as usize);
    }
    offsets.push(file_size);
    Some(offsets)
}

/// Checks if a buffer is a valid .dat datafile (header rules only).
fn is_dat_buffer(buffer: &[u8]) -> bool {
    parse_dat_offsets(buffer).is_some()
}

/// Checks if a file is a valid .dat datafile.
pub fn datafile_check(file: &mut (impl Read + Seek)) -> bool {
    log_printf(
        LogType::Verbose,
        "datafile_check: Checking if file is a valid .dat datafile",
    );

    let file_size = file.seek(SeekFrom::End(0)).unwrap_or(0);
    let _ = file.seek(SeekFrom::Start(0));

    log_printf(
        LogType::Verbose,
        &format!("datafile_check: File size {}", file_size),
    );

    let mut buf = Vec::with_capacity(file_size as usize);
    if file.read_to_end(&mut buf).is_err() {
        return false;
    }
    let _ = file.seek(SeekFrom::Start(0));

    if parse_dat_offsets(&buf).is_none() {
        log_printf(
            LogType::Verbose,
            "datafile_check: File is not a valid .dat datafile",
        );
        return false;
    }

    log_printf(
        LogType::Verbose,
        "datafile_check: File is a valid .dat datafile",
    );
    true
}

/// Finds file extension by checking header and footer magic bytes.
pub fn get_file_extension(file: &mut (impl Read + Seek)) -> String {
    if datafile_check(file) {
        return "dat".to_string();
    }

    let _ = file.seek(SeekFrom::Start(0));
    let mut header = [0u8; 32];
    let _ = file.read_exact(&mut header);

    let _ = file.seek(SeekFrom::End(-32));
    let mut footer = [0u8; 32];
    let _ = file.read_exact(&mut footer);

    let _ = file.seek(SeekFrom::Start(0));

    let ext = find_file_extension_header(&header);
    if ext == "bin" {
        find_file_extension_footer(&footer).to_string()
    } else {
        ext.to_string()
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub enum DatExpansion {
    #[default]
    Keep,
    Expand,
}

pub fn extract_dat_buffer_to_dir(
    buffer: &[u8],
    output_dir: &Path,
    expansion: DatExpansion,
) -> Result<Vec<String>, String> {
    log_printf(
        LogType::Verbose,
        &format!(
            "extract_dat_buffer_to_dir: Extracting to {}",
            output_dir.display()
        ),
    );

    if !is_dat_buffer(buffer) {
        return Err("Not a valid .dat buffer".into());
    }

    let file_count = u32::from_le_bytes([buffer[0], buffer[1], buffer[2], buffer[3]]) as usize;

    log_printf(
        LogType::Info,
        &format!("extract_dat_buffer_to_dir: File count {}", file_count),
    );

    let offsets = parse_dat_offsets(buffer).ok_or("Not a valid .dat buffer")?;

    // validate ascending order — guard against misidentified buffers
    for i in 0..offsets.len() - 1 {
        if offsets[i] >= offsets[i + 1] {
            return Err(format!(
                "Non-ascending offsets at entry {} ({} >= {}), not a valid .dat",
                i,
                offsets[i],
                offsets[i + 1]
            ));
        }
    }

    fs::create_dir_all(output_dir)
        .map_err(|e| format!("Failed to create output directory: {}", e))?;

    let mut filenames = Vec::new();

    for file_idx in 0..file_count {
        let slice = &buffer[offsets[file_idx]..offsets[file_idx + 1]];

        // detect extension from the slice
        let ext = {
            let mut cursor = Cursor::new(slice);
            get_file_extension(&mut cursor)
        };

        let new_name = format!("{:08}.{}", file_idx, ext);

        if ext == "dat" && expansion == DatExpansion::Expand {
            // nested .dat -> create folder, recurse
            let dat_dir = output_dir.join(&new_name);
            fs::create_dir_all(&dat_dir)
                .map_err(|e| format!("Failed to create dat subdirectory: {}", e))?;
            extract_dat_buffer_to_dir(slice, &dat_dir, expansion)?;
            log_printf(
                LogType::Info,
                &format!(
                    "extract_dat_buffer_to_dir: Extracted nested dat {} ({} entries)",
                    new_name,
                    u32::from_le_bytes([slice[0], slice[1], slice[2], slice[3]])
                ),
            );
        } else {
            // regular file -> write directly
            let file_path = output_dir.join(&new_name);
            fs::write(&file_path, slice)
                .map_err(|e| format!("Failed to write file {}: {}", new_name, e))?;
        }

        filenames.push(new_name);
    }

    log_printf(LogType::Verbose, "extract_dat_buffer_to_dir: Done");
    Ok(filenames)
}

pub(crate) fn parse_index_prefix(name: &str) -> Option<u32> {
    let bytes = name.as_bytes();
    let mut len = 0usize;
    while len < bytes.len() && bytes[len] != b'.' {
        if !bytes[len].is_ascii_digit() || len >= 15 {
            return None;
        }
        len += 1;
    }
    if len == 0 {
        return None;
    }
    name[..len].parse::<u32>().ok()
}

/// Recursively rebuild a .dat container from a directory into a byte buffer.
pub fn rebuild_dat_from_dir(dir_path: &Path) -> Result<Vec<u8>, String> {
    log_printf(
        LogType::Info,
        &format!("rebuild_dat_from_dir: Building from {}", dir_path.display()),
    );

    let mut items: Vec<(u32, bool, String)> = Vec::new(); // (index, is_dir, full_path)

    let dir = fs::read_dir(dir_path)
        .map_err(|e| format!("Failed to read directory {}: {}", dir_path.display(), e))?;

    for entry in dir.flatten() {
        let name = entry.file_name();
        let name_str = name.to_string_lossy().to_string();

        if name_str.is_empty() || name_str.starts_with('.') {
            continue;
        }

        let index = match parse_index_prefix(&name_str) {
            Some(v) => v,
            None => continue,
        };

        let metadata = fs::metadata(entry.path()).map_err(|e| e.to_string())?;
        let is_dir = metadata.is_dir();

        items.push((index, is_dir, entry.path().to_string_lossy().to_string()));
    }

    if items.is_empty() {
        return Err(format!(
            "No indexed files found in '{}'",
            dir_path.display()
        ));
    }

    for i in 1..items.len() {
        let mut j = i;
        let key = items[i].clone();
        while j > 0 && items[j - 1].0 > key.0 {
            items[j] = items[j - 1].clone();
            j -= 1;
        }
        items[j] = key;
    }

    if items[0].0 != 0 {
        return Err(format!("First index in '{}' must be 0", dir_path.display()));
    }
    for i in 1..items.len() {
        if items[i].0 == items[i - 1].0 {
            return Err(format!(
                "Duplicate index {} in '{}'",
                items[i].0,
                dir_path.display()
            ));
        }
    }
    for i in 1..items.len() {
        if items[i].0 != items[i - 1].0 + 1 {
            return Err(format!(
                "Missing index between {} and {} in '{}'",
                items[i - 1].0,
                items[i].0,
                dir_path.display()
            ));
        }
    }

    // build data buffers
    let mut data_buffers: Vec<Vec<u8>> = Vec::with_capacity(items.len());

    for (index, is_dir, path) in &items {
        if *is_dir {
            let nested = rebuild_dat_from_dir(Path::new(path))?;
            log_printf(
                LogType::Info,
                &format!(
                    "rebuild_dat_from_dir: Rebuilt nested dat {} ({} bytes)",
                    index,
                    nested.len()
                ),
            );
            data_buffers.push(nested);
        } else {
            let data =
                fs::read(path).map_err(|e| format!("Failed to read file {}: {}", path, e))?;
            data_buffers.push(data);
        }
    }

    // build .dat container in memory
    let item_count = items.len() as u32;
    let mut output: Vec<u8> = Vec::new();

    // write count
    output.extend_from_slice(&item_count.to_le_bytes());

    // placeholder offsets
    let header_size = (4 + item_count as usize * 4) as u64;
    output.resize(header_size as usize, 0);

    let align_padding = (0x10 - (header_size % 0x10)) % 0x10;
    let extra_block = if header_size % 0x10 == 0 { 0x10 } else { 0 };
    let data_start = header_size + align_padding + 0x10 + extra_block;
    output.resize(data_start as usize, 0);

    // write each data blob, record offsets
    let mut offsets: Vec<u32> = Vec::with_capacity(items.len());
    for buf in &data_buffers {
        offsets.push(output.len() as u32);
        output.extend_from_slice(buf);
    }

    // write back the offsets
    for (i, &off) in offsets.iter().enumerate() {
        let pos = 4 + i * 4;
        output[pos..pos + 4].copy_from_slice(&off.to_le_bytes());
    }

    log_printf(
        LogType::Info,
        &format!(
            "rebuild_dat_from_dir: Built {} entries, {} bytes total",
            item_count,
            output.len()
        ),
    );

    Ok(output)
}

pub fn extract_datafile(
    dat_path: &Path,
    output_dir: &str,
    expansion: DatExpansion,
) -> Result<(), String> {
    extract_datafile_quiet(dat_path, output_dir, expansion)?;
    println!("All files extracted successfully");
    Ok(())
}

fn extract_datafile_quiet(
    dat_path: &Path,
    output_dir: &str,
    expansion: DatExpansion,
) -> Result<(), String> {
    let mut file = fs::File::open(dat_path)
        .map_err(|e| format!("Failed to open .dat file {}: {}", dat_path.display(), e))?;

    if !datafile_check(&mut file) {
        return Err("Not a valid .dat file".into());
    }

    let mut buffer = Vec::new();
    file.read_to_end(&mut buffer)
        .map_err(|e| format!("Failed to read .dat file: {}", e))?;

    extract_dat_buffer_to_dir(&buffer, Path::new(output_dir), expansion).map(|_| ())
}

fn cleanup_temp_dir(dir_path: &Path) {
    if let Ok(entries) = fs::read_dir(dir_path) {
        for entry in entries.flatten() {
            let p = entry.path();
            if p.is_dir() {
                let _ = fs::remove_dir_all(&p);
            } else {
                let _ = fs::remove_file(&p);
            }
        }
    }
    let _ = fs::remove_dir(dir_path);
}

pub fn extract_datafile_range(
    in_dir: &str,
    start_idx: u32,
    end_idx: u32,
    output_dir: &str,
    ext: &str,
    expansion: DatExpansion,
) -> Result<(), String> {
    let total = end_idx - start_idx + 1;
    let mut file_counter: u32 = 0;

    log_printf(
        LogType::Info,
        &format!(
            "extract_datafile_range: Range {} {} {}",
            start_idx, end_idx, output_dir
        ),
    );

    fs::create_dir_all(output_dir)
        .map_err(|e| format!("Failed to create directory {}: {}", output_dir, e))?;

    for i in start_idx..=end_idx {
        let dat_path = Path::new(in_dir).join(format!("{:08}.dat", i));

        if !dat_path.exists() {
            log_printf(
                LogType::Warning,
                &format!(
                    "extract_datafile_range: Skipping (not found) {}",
                    dat_path.display()
                ),
            );
            println!(
                "[{}/{}] Skipping {} (not found)",
                i - start_idx + 1,
                total,
                dat_path.display()
            );
            continue;
        }

        let temp_dir_name = format!("_cdr_temp_{}", i);
        let temp_dir = Path::new(&temp_dir_name);

        log_printf(
            LogType::Info,
            &format!("extract_datafile_range: Extracting {}", dat_path.display()),
        );
        println!(
            "[{}/{}] Extracting {}...",
            i - start_idx + 1,
            total,
            dat_path.display()
        );

        let result = extract_datafile_quiet(&dat_path, &temp_dir_name, expansion);
        if let Err(e) = result {
            log_printf(
                LogType::Warning,
                &format!(
                    "extract_datafile_range: Failed to extract {}: {}",
                    dat_path.display(),
                    e
                ),
            );
            cleanup_temp_dir(temp_dir);
            continue;
        }

        let entries = match fs::read_dir(temp_dir) {
            Ok(d) => d,
            Err(_) => {
                cleanup_temp_dir(temp_dir);
                continue;
            }
        };

        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_string();
            let dot = match name.rfind('.') {
                Some(d) => d,
                None => continue,
            };
            if &name[dot + 1..] != ext {
                continue;
            }

            let src_path = entry.path();
            let dst_path = Path::new(output_dir).join(format!("{}.{}", file_counter, ext));

            if fs::copy(&src_path, &dst_path).is_err() {
                continue;
            }

            println!("  -> {} -> {}.{}", name, file_counter, ext);
            log_printf(
                LogType::Info,
                &format!(
                    "extract_datafile_range: {} -> {}.{}",
                    name, file_counter, ext
                ),
            );
            file_counter += 1;
        }

        cleanup_temp_dir(temp_dir);
    }

    println!(
        "\nDone! {} .{} files extracted to {}",
        file_counter, ext, output_dir
    );
    log_printf(
        LogType::Info,
        &format!(
            "extract_datafile_range: Done, {} files extracted to {}",
            file_counter, output_dir
        ),
    );
    Ok(())
}

pub fn build_datafile(input_dir: &str, output_filename: &str) -> Result<(), String> {
    let data = rebuild_dat_from_dir(Path::new(input_dir))?;

    if let Err(e) = fs::write(output_filename, &data) {
        let _ = fs::remove_file(output_filename);
        return Err(format!("Failed to create output .dat file: {}", e));
    }

    println!("Datafile built successfully: {}", output_filename);
    log_printf(
        LogType::Info,
        &format!(
            "build_datafile: Datafile built successfully: {}",
            output_filename
        ),
    );
    Ok(())
}
