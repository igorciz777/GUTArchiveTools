use std::fs;
use std::io::{Cursor, Read, Seek, SeekFrom};
use std::path::Path;

use crate::headers::{find_file_extension_footer, find_file_extension_header};
use crate::log::{log_printf, LogType};

/// Checks if a buffer starts with a valid .dat datafile header.
fn is_dat_buffer(buffer: &[u8]) -> bool {
    if buffer.len() < 8 {
        return false;
    }

    let file_size = buffer.len() as u32;
    let file_count = u32::from_le_bytes([buffer[0], buffer[1], buffer[2], buffer[3]]);

    if file_count == 0 || file_count > 100_000 {
        return false;
    }

    let header_end = 4u32 + file_count * 4;
    if header_end > file_size {
        return false;
    }

    for i in 0..file_count {
        let offset_pos = (4 + i * 4) as usize;
        let offset = u32::from_le_bytes([
            buffer[offset_pos],
            buffer[offset_pos + 1],
            buffer[offset_pos + 2],
            buffer[offset_pos + 3],
        ]);
        if offset == 0 || offset >= file_size || offset == 0xFFFFFFFF {
            return false;
        }
    }

    true
}

/// Checks if a file is a valid .dat datafile.
pub fn datafile_check(file: &mut (impl Read + Seek)) -> bool {
    log_printf(LogType::Verbose, "datafile_check: Checking if file is a valid .dat datafile");

    let file_size = file.seek(SeekFrom::End(0)).unwrap_or(0);
    file.seek(SeekFrom::Start(0)).unwrap_or(0);

    log_printf(LogType::Verbose, &format!("datafile_check: File size {}", file_size));

    if file_size < 0x8 {
        log_printf(LogType::Verbose, "datafile_check: File size is too small to be a valid .dat file");
        return false;
    }

    let mut header = [0u8; 4];
    if file.read_exact(&mut header).is_err() {
        return false;
    }
    file.seek(SeekFrom::Start(0)).unwrap_or(0);

    let file_size_u32 = file_size as u32;
    let file_count = u32::from_le_bytes(header);

    if file_count == 0 || file_count > 100_000 {
        log_printf(LogType::Verbose, &format!("datafile_check: Invalid file count {}", file_count));
        return false;
    }

    let header_end = 4u32 + file_count * 4;
    if header_end > file_size_u32 {
        return false;
    }

    for i in 0..file_count {
        let mut offset_bytes = [0u8; 4];
        let offset_pos = 4 + i * 4;
        if file.seek(SeekFrom::Start(offset_pos as u64)).is_err() {
            return false;
        }
        if file.read_exact(&mut offset_bytes).is_err() {
            return false;
        }
        let offset = u32::from_le_bytes(offset_bytes);
        if offset == 0 || offset >= file_size_u32 || offset == 0xFFFFFFFF {
            log_printf(LogType::Verbose, &format!("datafile_check: Invalid offset for file {}", i));
            return false;
        }
    }

    let _ = file.seek(SeekFrom::Start(0));
    log_printf(LogType::Verbose, "datafile_check: File is a valid .dat datafile");
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

/// Recursively extract a .dat buffer into a directory.
/// Each entry becomes either a file or a nested .dat folder.
pub fn extract_dat_buffer_to_dir(
    buffer: &[u8],
    output_dir: &Path,
) -> Result<Vec<String>, String> {
    log_printf(LogType::Verbose, &format!(
        "extract_dat_buffer_to_dir: Extracting to {}",
        output_dir.display()
    ));

    if !is_dat_buffer(buffer) {
        return Err("Not a valid .dat buffer".into());
    }

    let file_size = buffer.len() as u32;
    let file_count = u32::from_le_bytes([buffer[0], buffer[1], buffer[2], buffer[3]]);

    log_printf(LogType::Info, &format!(
        "extract_dat_buffer_to_dir: File count {}", file_count
    ));

    let mut offsets = Vec::with_capacity(file_count as usize + 1);
    for i in 0..file_count {
        let offset_pos = (4 + i * 4) as usize;
        let offset = u32::from_le_bytes([
            buffer[offset_pos],
            buffer[offset_pos + 1],
            buffer[offset_pos + 2],
            buffer[offset_pos + 3],
        ]);
        if offset == 0 || offset >= file_size || offset == 0xFFFFFFFF {
            log_printf(LogType::Error, &format!(
                "extract_dat_buffer_to_dir: Invalid offset in .dat buffer for file {}", i
            ));
            return Err("Invalid offset in .dat buffer".into());
        }
        offsets.push(offset as usize);
    }
    offsets.push(file_size as usize);

    // validate ascending order — guard against misidentified buffers
    for i in 0..offsets.len() - 1 {
        if offsets[i] >= offsets[i + 1] {
            return Err(format!(
                "Non-ascending offsets at entry {} ({} >= {}), not a valid .dat",
                i, offsets[i], offsets[i + 1]
            ));
        }
    }

    fs::create_dir_all(output_dir)
        .map_err(|e| format!("Failed to create output directory: {}", e))?;

    let mut filenames = Vec::new();

    for file_idx in 0..file_count as usize {
        let slice = &buffer[offsets[file_idx]..offsets[file_idx + 1]];

        // detect extension from the slice
        let ext = {
            let mut cursor = Cursor::new(slice);
            get_file_extension(&mut cursor)
        };

        let new_name = format!("{:08}.{}", file_idx, ext);

        if ext == "dat" {
            // nested .dat -> create folder, recurse
            let dat_dir = output_dir.join(&new_name);
            fs::create_dir_all(&dat_dir)
                .map_err(|e| format!("Failed to create dat subdirectory: {}", e))?;
            extract_dat_buffer_to_dir(slice, &dat_dir)?;
            log_printf(LogType::Info, &format!(
                "extract_dat_buffer_to_dir: Extracted nested dat {} ({} entries)",
                new_name,
                u32::from_le_bytes([slice[0], slice[1], slice[2], slice[3]])
            ));
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

/// Recursively rebuild a .dat container from a directory into a byte buffer.
/// Directories ending with ".dat" are recursively packed into nested .dat containers.
pub fn rebuild_dat_from_dir(dir_path: &Path) -> Result<Vec<u8>, String> {
    log_printf(LogType::Info, &format!(
        "rebuild_dat_from_dir: Building from {}",
        dir_path.display()
    ));

    let mut items: Vec<(u32, bool, String)> = Vec::new(); // (index, is_dat_dir, full_path)

    let dir = fs::read_dir(dir_path)
        .map_err(|e| format!("Failed to read directory {}: {}", dir_path.display(), e))?;

    for entry in dir.flatten() {
        let name = entry.file_name();
        let name_str = name.to_string_lossy().to_string();

        if name_str.is_empty() || name_str.starts_with('.') {
            continue;
        }

        // parse numeric prefix
        let num_str: String = name_str.chars().take_while(|c| c.is_ascii_digit()).collect();
        if num_str.is_empty() {
            continue;
        }

        let index: u32 = match num_str.parse() {
            Ok(v) => v,
            Err(_) => continue,
        };

        let metadata = fs::metadata(entry.path()).map_err(|e| e.to_string())?;
        let is_dat_dir = metadata.is_dir() && name_str.ends_with(".dat");

        items.push((index, is_dat_dir, entry.path().to_string_lossy().to_string()));
    }

    if items.is_empty() {
        return Err(format!("No indexed files found in '{}'", dir_path.display()));
    }

    items.sort_by_key(|a| a.0);

    // validate sequential indices starting from 0
    if items[0].0 != 0 {
        return Err(format!("First index in '{}' must be 0", dir_path.display()));
    }
    for i in 1..items.len() {
        if items[i].0 != items[i - 1].0 + 1 {
            return Err(format!(
                "Missing index between {} and {} in '{}'",
                items[i - 1].0, items[i].0, dir_path.display()
            ));
        }
    }

    // build data buffers
    let mut data_buffers: Vec<Vec<u8>> = Vec::with_capacity(items.len());

    for (index, is_dat_dir, path) in &items {
        if *is_dat_dir {
            let sub_path = Path::new(path);
            let nested = rebuild_dat_from_dir(sub_path)?;
            log_printf(LogType::Info, &format!(
                "rebuild_dat_from_dir: Rebuilt nested dat {} ({} bytes)",
                index, nested.len()
            ));
            data_buffers.push(nested);
        } else {
            let data = fs::read(path)
                .map_err(|e| format!("Failed to read file {}: {}", path, e))?;
            data_buffers.push(data);
        }
    }

    // build .dat container in memory
    let item_count = items.len() as u32;
    let mut output: Vec<u8> = Vec::new();

    // write count
    output.extend_from_slice(&item_count.to_le_bytes());

    // placeholder offsets (data starts right after header)
    let header_size = 4 + item_count as usize * 4;
    output.resize(header_size, 0);

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

    log_printf(LogType::Info, &format!(
        "rebuild_dat_from_dir: Built {} entries, {} bytes total",
        item_count,
        output.len()
    ));

    Ok(output)
}
