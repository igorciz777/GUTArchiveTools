use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;

use crate::file_io::{xread, xread32le, xwrite};
use crate::headers::{find_file_extension_footer, find_file_extension_header};
use crate::log::{log_printf, LogType};

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

    let file_count = match xread32le(file) {
        Ok(c) => c,
        Err(_) => return false,
    };

    if file_count == 0 || file_count > ((file_size as u32 / 4) + 4) {
        log_printf(LogType::Verbose, &format!("datafile_check: Invalid file count {}", file_count));
        return false;
    }

    for i in 0..file_count {
        let offset = match xread32le(file) {
            Ok(o) => o,
            Err(_) => return false,
        };
        if offset == 0 || offset >= file_size as u32 || offset == 0xFFFFFFFF {
            log_printf(LogType::Verbose, &format!("datafile_check: Invalid offset for file {}", i));
            return false;
        }
        if file_count == 0x0F && offset == 0x29 && i < 1 {
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
    let _ = xread(file, &mut header, false);

    let _ = file.seek(SeekFrom::End(-32));
    let mut footer = [0u8; 32];
    let _ = xread(file, &mut footer, false);

    let _ = file.seek(SeekFrom::Start(0));

    let ext = find_file_extension_header(&header);
    if ext == "bin" {
        find_file_extension_footer(&footer).to_string()
    } else {
        ext.to_string()
    }
}

fn extract_datafile_to_dir_inner(
    datafile: &mut (impl Read + Seek),
    output_dir: &Path,
) -> Result<Vec<String>, String> {
    log_printf(LogType::Verbose, "_extract_datafile_to_dir: Starting extraction");

    if !datafile_check(datafile) {
        return Err("Not a valid .dat file".into());
    }

    let file_size = datafile.seek(SeekFrom::End(0)).map_err(|e| e.to_string())?;
    datafile.seek(SeekFrom::Start(0)).map_err(|e| e.to_string())?;

    let file_count = xread32le(datafile).map_err(|e| e.to_string())?;
    log_printf(LogType::Info, &format!("_extract_datafile_to_dir: File count {}", file_count));

    let mut offsets = Vec::with_capacity(file_count as usize + 1);
    for i in 0..file_count {
        let offset = xread32le(datafile).map_err(|e| e.to_string())?;
        if offset == 0 || offset >= file_size as u32 || offset == 0xFFFFFFFF {
            log_printf(LogType::Error, &format!("_extract_datafile_to_dir: Invalid offset in .dat file for file {}", i));
            return Err("Invalid offset in .dat file".into());
        }
        offsets.push(offset);
    }
    offsets.push(file_size as u32);

    fs::create_dir_all(output_dir).map_err(|e| format!("Failed to create output directory: {}", e))?;

    let mut filenames = Vec::new();

    for file_idx in 0..file_count {
        let temp_name = format!("temp{}", file_idx);
        let temp_path = output_dir.join(&temp_name);

        let actual_length = (offsets[file_idx as usize + 1] - offsets[file_idx as usize]) as usize;
        let actual_offset = offsets[file_idx as usize] as u64;

        datafile.seek(SeekFrom::Start(actual_offset)).map_err(|e| e.to_string())?;
        let mut file_data = vec![0u8; actual_length];
        xread(datafile, &mut file_data, false).map_err(|e| e.to_string())?;

        fs::write(&temp_path, &file_data).map_err(|e| format!("Failed to write temp file: {}", e))?;

        let mut temp_file = fs::File::open(&temp_path).map_err(|e| e.to_string())?;
        let ext = get_file_extension(&mut temp_file);
        drop(temp_file);

        let new_name = format!("{:08}.{}", file_idx, ext);
        let new_path = output_dir.join(&new_name);
        if fs::rename(&temp_path, &new_path).is_err() {
            log_printf(LogType::Error, &format!(
                "_extract_datafile_to_dir: Failed to rename output file for file {}", file_idx
            ));
            let _ = fs::remove_file(&temp_path);
        }
        filenames.push(new_name);
    }

    log_printf(LogType::Verbose, "_extract_datafile_to_dir: All files extracted successfully");
    Ok(filenames)
}

/// Extracts all entries from a .dat datafile to a specified directory.
pub fn extract_datafile(mut datafile: File, output_dir: &Path) -> Result<(), String> {
    let result = extract_datafile_to_dir_inner(&mut datafile, output_dir)?;
    drop(datafile);
    println!("All files extracted successfully");
    log_printf(LogType::Info, &format!(
        "extract_datafile: Extracted {} files to {}",
        result.len(),
        output_dir.display()
    ));
    Ok(())
}

fn cleanup_temp_dir(dir_path: &Path) {
    if let Ok(entries) = fs::read_dir(dir_path) {
        for entry in entries.flatten() {
            let path = entry.path();
            let _ = fs::remove_file(&path);
        }
        let _ = fs::remove_dir(dir_path);
    }
}

/// Extracts a range of .dat containers and collects files matching an extension.
pub fn extract_datafile_range(
    in_dir: &str,
    start_idx: u32,
    end_idx: u32,
    output_dir: &str,
    ext: &str,
) -> Result<(), String> {
    let total = end_idx - start_idx + 1;
    let mut file_counter = 0;

    log_printf(LogType::Info, &format!(
        "extract_datafile_range: Range {} {} {}", start_idx, end_idx, output_dir
    ));

    let out_path = Path::new(output_dir);
    fs::create_dir_all(out_path).map_err(|e| format!("Failed to create output directory: {}", e))?;

    for i in start_idx..=end_idx {
        let dat_path = format!("{}/{:08}.dat", in_dir, i);
        let dat_path = Path::new(&dat_path);

        let mut datafile = match fs::File::open(dat_path) {
            Ok(f) => f,
            Err(_) => {
                log_printf(LogType::Warning, &format!(
                    "extract_datafile_range: Skipping (not found) {}", dat_path.display()
                ));
                println!("[{}/{}] Skipping {} (not found)", i - start_idx + 1, total, dat_path.display());
                continue;
            }
        };

        let temp_dir = format!("_cdr_temp_{}", i);
        let temp_path = Path::new(&temp_dir);

        log_printf(LogType::Info, &format!("extract_datafile_range: Extracting {}", dat_path.display()));
        println!("[{}/{}] Extracting {}...", i - start_idx + 1, total, dat_path.display());

        let file_list = match extract_datafile_to_dir_inner(&mut datafile, temp_path) {
            Ok(files) => files,
            Err(e) => {
                log_printf(LogType::Warning, &format!(
                    "extract_datafile_range: Failed to extract {}: {}", dat_path.display(), e
                ));
                cleanup_temp_dir(temp_path);
                continue;
            }
        };

        for fname in &file_list {
            let dot = fname.rfind('.');
            let file_ext = dot.map(|p| &fname[p + 1..]).unwrap_or("");
            if file_ext != ext {
                continue;
            }

            let src_path = temp_path.join(fname);
            let dst_name = format!("{}/{}.{}", output_dir, file_counter, ext);
            let dst_path = Path::new(&dst_name);

            let src_data = match fs::read(&src_path) {
                Ok(d) => d,
                Err(_) => continue,
            };
            if fs::write(dst_path, &src_data).is_err() {
                continue;
            }

            println!("  -> {} -> {}.{}", fname, file_counter, ext);
            log_printf(LogType::Info, &format!(
                "extract_datafile_range: {} -> {}.{}", fname, file_counter, ext
            ));
            file_counter += 1;
        }

        cleanup_temp_dir(temp_path);
    }

    println!("\nDone! {} .{} files extracted to {}", file_counter, ext, output_dir);
    log_printf(LogType::Info, &format!(
        "extract_datafile_range: Done, files extracted {} {}", file_counter, output_dir
    ));
    Ok(())
}

struct DatBuildItem {
    index: u32,
    is_dir: bool,
    path: String,
}

/// Builds a new .dat datafile from a directory of indexed files.
pub fn build_datafile(input_dir: &str, output_filename: &str) -> Result<(), String> {
    let output_path = Path::new(output_filename);
    let mut output_file = fs::File::create(output_path)
        .map_err(|e| format!("Failed to create output .dat file: {}", e))?;

    log_printf(LogType::Info, &format!(
        "build_datafile: Building datafile from directory {}", input_dir
    ));

    let dir = match fs::read_dir(input_dir) {
        Ok(d) => d,
        Err(e) => return Err(format!("Failed to open input directory: {}", e)),
    };

    let mut items: Vec<DatBuildItem> = Vec::new();

    for entry in dir.flatten() {
        let name = entry.file_name();
        let name_str = name.to_string_lossy().to_string();

        if name_str.is_empty() || name_str.starts_with('.') {
            continue;
        }

        let dot_pos = name_str.find('.');
        let num_str = match dot_pos {
            Some(p) => &name_str[..p],
            None => continue,
        };

        if num_str.is_empty() || !num_str.bytes().all(|b| b.is_ascii_digit()) {
            continue;
        }

        let file_index: u32 = num_str.parse().unwrap_or(0);
        let full_path = entry.path();
        let metadata = fs::metadata(&full_path).map_err(|e| e.to_string())?;

        items.push(DatBuildItem {
            index: file_index,
            is_dir: metadata.is_dir(),
            path: full_path.to_string_lossy().to_string(),
        });
    }

    if items.is_empty() {
        return Err(format!("Error: No indexed files found in '{}'", input_dir));
    }

    items.sort_by_key(|a| a.index);

    for i in 1..items.len() {
        if items[i].index == items[i - 1].index {
            return Err(format!(
                "Error: Duplicate index {} in '{}'",
                items[i].index, input_dir
            ));
        }
    }

    if items[0].index != 0 {
        return Err(format!(
            "Error: First index in '{}' must be 0",
            input_dir
        ));
    }

    for i in 1..items.len() {
        if items[i].index != items[i - 1].index + 1 {
            return Err(format!(
                "Error: Missing index between {} and {} in '{}'",
                items[i - 1].index, items[i].index, input_dir
            ));
        }
    }

    let item_count = items.len() as u32;
    xwrite(&mut output_file, &item_count.to_le_bytes()).map_err(|e| e.to_string())?;
    for _ in 0..item_count {
        xwrite(&mut output_file, &0u32.to_le_bytes()).map_err(|e| e.to_string())?;
    }

    // align to 0x10 boundary + extra 0x10
    let toc_end = output_file.stream_position().map_err(|e| e.to_string())?;
    let align_padding = (0x10 - (toc_end % 0x10)) % 0x10;
    let mut total_padding = align_padding + 0x10;
    let zero_line = [0u8; 0x10];

    while total_padding >= 0x10 {
        xwrite(&mut output_file, &zero_line).map_err(|e| e.to_string())?;
        total_padding -= 0x10;
    }

    let mut offsets = vec![0u32; item_count as usize];

    for item_idx in 0..item_count as usize {
        let offset_pos = output_file.stream_position().map_err(|e| e.to_string())?;
        if offset_pos > 0xFFFF_FFFF {
            return Err("Error: Output file too large".into());
        }
        offsets[item_idx] = offset_pos as u32;

        let mut buffer = [0u8; 0x4000];

        if items[item_idx].is_dir {
            let nested_path = format!("{}.nested", output_filename);
            build_datafile(&items[item_idx].path, &nested_path)?;

            let mut nested_file = fs::File::open(&nested_path)
                .map_err(|e| format!("Failed to open nested .dat file: {}", e))?;

            loop {
                let n = xread(&mut nested_file, &mut buffer, true).map_err(|e| e.to_string())?;
                if n == 0 {
                    break;
                }
                xwrite(&mut output_file, &buffer[..n]).map_err(|e| e.to_string())?;
            }
            drop(nested_file);
            let _ = fs::remove_file(&nested_path);
        } else {
            let mut src = fs::File::open(&items[item_idx].path)
                .map_err(|e| format!("Failed to open input file: {}", e))?;

            loop {
                let n = xread(&mut src, &mut buffer, true).map_err(|e| e.to_string())?;
                if n == 0 {
                    break;
                }
                xwrite(&mut output_file, &buffer[..n]).map_err(|e| e.to_string())?;
            }
        }
    }

    // write offset table
    let end_pos = output_file.stream_position().map_err(|e| e.to_string())?;
    output_file
        .seek(SeekFrom::Start(4))
        .map_err(|e| e.to_string())?;

    for &off in &offsets {
        xwrite(&mut output_file, &off.to_le_bytes()).map_err(|e| e.to_string())?;
    }

    output_file
        .seek(SeekFrom::Start(end_pos))
        .map_err(|e| e.to_string())?;

    println!("Datafile built successfully: {}", output_filename);
    log_printf(LogType::Info, &format!(
        "build_datafile: Datafile built successfully {}", output_filename
    ));
    Ok(())
}
