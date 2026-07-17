use std::cell::Cell;
use std::fs;
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::Path;

use indicatif::{ProgressBar, ProgressStyle};

use crate::datafile::get_file_extension;
use crate::file_io::{
    swap_uint32, xread, xread32be, xread32le, xwrite, xwrite32be,
};
use crate::log::{log_printf, LogType};
use crate::ucl::do_compress;

thread_local! {
    pub static GAME_ID: Cell<GameId> = const { Cell::new(GameId::Default) };
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum GameId {
    Default = 0,
    KB3 = 1,
    ITC = 2,
    KB1T = 3,
    KB2D = 4,
}

pub fn set_game_id(arg: &str) {
    let id = match arg {
        "-0" => GameId::KB3,
        "-2" => GameId::ITC,
        "-3" => GameId::KB1T,
        "-4" => GameId::KB2D,
        _ => return,
    };
    GAME_ID.with(|g| g.set(id));
    log_printf(LogType::Info, &format!("set_game_id: Set game ID to {:?}", id));
}

#[derive(Clone, Debug)]
pub struct TocEntry {
    pub start_offset: u32,
    pub end_offset: u32,
    pub compressed_size: u32,
    pub decompressed_size: u32,
    pub zero_field: u32,
}

#[derive(Clone)]
pub struct RebuildEntry {
    pub importing: bool,
    pub compressed: bool,
    pub skip: bool,
    pub block_size: u32,
    pub infilename: String,
    pub toc_entry: TocEntry,
}

fn make_progress(len: u64) -> ProgressBar {
    let pb = ProgressBar::new(len);
    pb.set_style(
        ProgressStyle::default_bar()
            .template("[{bar:50}] {percent:>3}%")
            .unwrap()
            .progress_chars("=> "),
    );
    pb
}

pub fn get_toc_entries(
    toc_file: &mut (impl Read + Seek),
) -> Result<(u32, Vec<TocEntry>), String> {
    log_printf(LogType::Verbose, "get_TOC_entries: Reading TOC entries");

    let mut file_count = xread32le(toc_file).map_err(|e| e.to_string())?;

    let game_id = GAME_ID.with(|g| g.get());
    if game_id == GameId::ITC {
        file_count = swap_uint32(file_count);
    }

    log_printf(LogType::Info, &format!("get_TOC_entries: File count: {}", file_count));

    toc_file.seek(SeekFrom::Start(0x10)).map_err(|e| e.to_string())?;

    let mut entries = Vec::with_capacity(file_count as usize);

    for i in 0..file_count {
        let mut entry = TocEntry {
            start_offset: 0,
            end_offset: 0,
            compressed_size: 0,
            decompressed_size: 0,
            zero_field: 0,
        };

        match game_id {
            GameId::KB3 => {
                entry.start_offset = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.compressed_size = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.decompressed_size = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.zero_field = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.end_offset = 1 + (entry.compressed_size / 0x800);
            }
            GameId::ITC => {
                entry.start_offset = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.compressed_size = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.compressed_size = swap_uint32(entry.compressed_size);
                entry.decompressed_size = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.decompressed_size = swap_uint32(entry.decompressed_size);
                entry.zero_field = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.end_offset = if entry.zero_field != 0 {
                    0
                } else {
                    swap_uint32(1 + (entry.compressed_size / 0x800))
                };
            }
            GameId::KB1T => {
                entry.start_offset = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.compressed_size = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.end_offset = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.zero_field = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.decompressed_size = 0;
                if entry.compressed_size == 0 {
                    entry.zero_field = 1;
                }
            }
            _ => {
                entry.start_offset = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.compressed_size = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.end_offset = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.zero_field = xread32le(toc_file).map_err(|e| e.to_string())?;
                entry.decompressed_size = xread32le(toc_file).map_err(|e| e.to_string())?;
            }
        }

        log_printf(LogType::Info, &format!("get_TOC_entries: TOC entry {}", i));
        log_printf(LogType::Info, &format!("  Start Offset: {:08x}", entry.start_offset));
        log_printf(LogType::Info, &format!("  Compressed Size: {:08x}", entry.compressed_size));
        log_printf(LogType::Info, &format!("  Decompressed Size: {:08x}", entry.decompressed_size));
        log_printf(LogType::Info, &format!("  End Offset: {:08x}", entry.end_offset));
        log_printf(LogType::Info, &format!("  Zero Field: {:08x}", entry.zero_field));

        entries.push(entry);
    }

    Ok((file_count, entries))
}

pub fn write_toc_entries(
    toc_file: &mut (impl Write + Seek),
    entries: &[RebuildEntry],
) -> Result<(), String> {
    log_printf(LogType::Verbose, "write_TOC_entries: Writing TOC entries");

    toc_file.seek(SeekFrom::Start(0x10)).map_err(|e| e.to_string())?;

    let game_id = GAME_ID.with(|g| g.get());

    for (i, entry) in entries.iter().enumerate() {
        let te = &entry.toc_entry;
        match game_id {
            GameId::KB3 => {
                xwrite(toc_file, &te.start_offset.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.compressed_size.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.decompressed_size.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.zero_field.to_le_bytes()).map_err(|e| e.to_string())?;
            }
            GameId::ITC => {
                xwrite(toc_file, &te.start_offset.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite32be(toc_file, te.compressed_size).map_err(|e| e.to_string())?;
                xwrite32be(toc_file, te.decompressed_size).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.zero_field.to_le_bytes()).map_err(|e| e.to_string())?;
            }
            GameId::KB1T => {
                xwrite(toc_file, &te.start_offset.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.compressed_size.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.end_offset.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.zero_field.to_le_bytes()).map_err(|e| e.to_string())?;
            }
            _ => {
                xwrite(toc_file, &te.start_offset.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.compressed_size.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.end_offset.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.zero_field.to_le_bytes()).map_err(|e| e.to_string())?;
                xwrite(toc_file, &te.decompressed_size.to_le_bytes()).map_err(|e| e.to_string())?;
            }
        }
        log_printf(LogType::Info, &format!("write_TOC_entries: Wrote TOC entry {}", i));
    }

    Ok(())
}

pub fn extract_gut_archive_entry(
    _toc_file: &mut (impl Read + Seek),
    dat_file: &mut (impl Read + Seek),
    out: &mut impl Write,
    entry: &TocEntry,
) -> Result<(), String> {
    log_printf(LogType::Info, &format!(
        "extract_GUTArchive_entry: Extracting entry {:08x} {:08x}",
        entry.start_offset, entry.end_offset
    ));

    let game_id = GAME_ID.with(|g| g.get());

    let actual_length: u64;
    let actual_offset: u64;

    if game_id == GameId::ITC {
        actual_length = swap_uint32(entry.end_offset) as u64 * 0x800;
        actual_offset = swap_uint32(entry.start_offset) as u64 * 0x800;
    } else {
        actual_length = entry.end_offset as u64 * 0x800;
        actual_offset = entry.start_offset as u64 * 0x800;
    }

    let compressed = entry.decompressed_size != 0;

    dat_file
        .seek(SeekFrom::Start(actual_offset))
        .map_err(|e| e.to_string())?;

    let mut file_data = vec![0u8; actual_length as usize];
    xread(dat_file, &mut file_data, false).map_err(|e| e.to_string())?;

    if !compressed {
        xwrite(out, &file_data).map_err(|e| e.to_string())?;
    } else {
        let comp_size = entry.compressed_size as usize;
        let mut temp_buf = std::io::Cursor::new(file_data[..comp_size].to_vec());

        let mut out_buf = std::io::Cursor::new(Vec::new());
        crate::ucl::do_decompress(&mut temp_buf, &mut out_buf)?;

        xwrite(out, &out_buf.into_inner()).map_err(|e| e.to_string())?;
    }

    log_printf(LogType::Info, &format!(
        "extract_GUTArchive_entry: Finished extracting entry {:08x} {:08x}",
        entry.start_offset, entry.end_offset
    ));
    Ok(())
}

pub fn extract_gut_archive_all(
    toc_file: &mut (impl Read + Seek),
    dat_file: &mut (impl Read + Seek),
    output_dir: &str,
    extract_dats: bool,
) -> Result<(), String> {
    log_printf(LogType::Info, "extract_GUTArchive_all: Reading TOC entries");

    let (_file_count, entries) = get_toc_entries(toc_file)?;
    let file_count = entries.len() as u32;

    if file_count == 0 {
        return Err("Failed to read TOC entries".into());
    }

    let out_path = Path::new(output_dir);
    fs::create_dir_all(out_path).map_err(|e| format!("Failed to create directory: {}", e))?;

    let game_id = GAME_ID.with(|g| g.get());

    let total = entries.len() as u64;
    let pb = make_progress(total);
    let mut processed = 0u64;

    for (file_idx, entry) in entries.iter().enumerate() {
        // skip logic
        if (entry.start_offset == 0
            && file_idx > 1
            && (game_id == GameId::KB3 || game_id == GameId::ITC || game_id == GameId::KB2D))
            || (entry.zero_field == 1 && game_id != GameId::KB3)
            || (file_idx == 0 && game_id == GameId::ITC)
        {
            continue;
        }

        let temp_name = format!("temp{}", file_idx);
        let temp_path = out_path.join(&temp_name);

        let mut out = fs::File::create(&temp_path)
            .map_err(|e| format!("Failed to create output file: {}", e))?;

        extract_gut_archive_entry(toc_file, dat_file, &mut out, entry)?;
        drop(out);

        processed += 1;
        pb.set_position(processed);

        let mut out_r = fs::File::open(&temp_path).map_err(|e| e.to_string())?;
        let ext = get_file_extension(&mut out_r);
        drop(out_r);

        let new_name = format!("{:08}.{}", file_idx, ext);
        let new_path = out_path.join(&new_name);
        if fs::rename(&temp_path, &new_path).is_err() {
            log_printf(LogType::Error, &format!(
                "extract_GUTArchive_all: Failed to rename output file for file {}", file_idx
            ));
            let _ = fs::remove_file(&temp_path);
        }

        if extract_dats && ext == "dat" {
            let dat_subdir = new_path.with_extension("");
            fs::create_dir_all(&dat_subdir).map_err(|e| format!(
                "extract_GUTArchive_all: Failed to create dat subdirectory: {}", e
            ))?;
            let mut dat_file = fs::File::open(&new_path).map_err(|e| format!(
                "extract_GUTArchive_all: Failed to open dat file: {}", e
            ))?;
            match crate::datafile::extract_datafile_to_dir_inner(&mut dat_file, &dat_subdir) {
                Ok(files) => {
                    log_printf(LogType::Info, &format!(
                        "extract_GUTArchive_all: Extracted {} files from {}", files.len(), new_name
                    ));
                }
                Err(e) => {
                    log_printf(LogType::Warning, &format!(
                        "extract_GUTArchive_all: Failed to extract dat {}: {}", new_name, e
                    ));
                    let _ = fs::remove_dir_all(&dat_subdir);
                }
            }
        }
    }

    pb.finish_and_clear();
    println!("All files extracted successfully");
    log_printf(LogType::Info, "extract_GUTArchive_all: All files extracted successfully");
    Ok(())
}

pub fn rebuild_gut_archive(
    toc_filename: &str,
    dat_filename: &str,
    input_dir: &str,
) -> Result<(), String> {
    use std::fs::OpenOptions;
    let mut toc_file = OpenOptions::new()
        .read(true)
        .write(true)
        .open(toc_filename)
        .map_err(|e| format!("Failed to open .toc file: {}", e))?;
    let mut dat_file = OpenOptions::new()
        .read(true)
        .open(dat_filename)
        .map_err(|e| format!("Failed to open .dat file: {}", e))?;

    let dir = match fs::read_dir(input_dir) {
        Ok(d) => d,
        Err(e) => return Err(format!("Failed to open input directory: {}", e)),
    };

    println!("Reading file info from TOC...");
    log_printf(LogType::Info, "rebuild_GUTArchive: Reading file info from TOC");

    let (file_count, toc_entries) = get_toc_entries(&mut toc_file)?;
    if file_count == 0 {
        return Err("Failed to read TOC entries".into());
    }

    let mut files: Vec<RebuildEntry> = Vec::with_capacity(file_count as usize);
    let game_id = GAME_ID.with(|g| g.get());

    for i in 0..file_count as usize {
        let mut re = RebuildEntry {
            importing: false,
            compressed: false,
            skip: false,
            block_size: 0,
            infilename: String::new(),
            toc_entry: toc_entries[i].clone(),
        };

        if game_id == GameId::KB3 {
            re.skip = false;
        }

        let actual_offset = if game_id == GameId::ITC {
            swap_uint32(toc_entries[i].start_offset) as u64 * 0x800
        } else {
            toc_entries[i].start_offset as u64 * 0x800
        };

        if (actual_offset == 0 && i > 1) || 
           (toc_entries[i].zero_field == 1 && game_id != GameId::KB3) {
            re.skip = true;
        }

        re.compressed = toc_entries[i].decompressed_size != 0;

        if re.compressed {
            dat_file.seek(SeekFrom::Start(actual_offset)).map_err(|e| e.to_string())?;
            dat_file.seek(SeekFrom::Current(14)).map_err(|e| e.to_string())?;
            re.block_size = xread32be(&mut dat_file).map_err(|e| e.to_string())?;
        }

        files.push(re);
    }

    let _ = dat_file.seek(SeekFrom::Start(0));

    for entry in dir.flatten() {
        let name = entry.file_name();
        let name_str = name.to_string_lossy().to_string();

        if !name_str.contains('.') || name_str.starts_with('.') {
            continue;
        }

        let dot_pos = name_str.find('.').unwrap();
        let index_str = &name_str[..dot_pos];
        let file_index: usize = index_str.parse().unwrap_or(0);

        println!("Processing file id:{}", file_index);
        log_printf(LogType::Info, &format!("rebuild_GUTArchive: Processing file id:{}", file_index));

        if file_index >= file_count as usize {
            println!("Invalid file index for file {}", name_str);
            log_printf(LogType::Warning, &format!(
                "rebuild_GUTArchive: Invalid file index for file {}", name_str
            ));
            continue;
        }

        files[file_index].importing = true;
        files[file_index].infilename = entry.path().to_string_lossy().to_string();
    }

    let new_dat_path = std::env::current_dir().map_err(|e| e.to_string())?.join("new.dat");
    let mut new_dat = fs::OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(true)
        .open(&new_dat_path)
        .map_err(|e| format!("Failed to create new .dat file: {}", e))?;

    println!("Rebuilding GUT Archive...");
    log_printf(LogType::Info, "rebuild_GUTArchive: Rebuilding GUT Archive");

    let pb = make_progress(file_count as u64);
    let mut additional_offset: u32 = 0;

    for file_index in 0..file_count as usize {
        if !files[file_index].importing {
            if !files[file_index].skip {
                // move other files from old dat to new dat with added offset
                let (old_off, new_off) = if game_id == GameId::ITC {
                    let o = swap_uint32(files[file_index].toc_entry.start_offset) as u64;
                    (o * 0x800, (o + additional_offset as u64) * 0x800)
                } else {
                    let o = files[file_index].toc_entry.start_offset as u64;
                    (o * 0x800, (o + additional_offset as u64) * 0x800)
                };
                dat_file.seek(SeekFrom::Start(old_off)).map_err(|e| e.to_string())?;
                new_dat.seek(SeekFrom::Start(new_off)).map_err(|e| e.to_string())?;

                let actual_length = if game_id == GameId::ITC {
                    swap_uint32(files[file_index].toc_entry.end_offset) as u64 * 0x800
                } else {
                    files[file_index].toc_entry.end_offset as u64 * 0x800
                } as usize;


                let mut file_data = vec![0u8; actual_length];
                xread(&mut dat_file, &mut file_data, false).map_err(|e| e.to_string())?;
                xwrite(&mut new_dat, &file_data).map_err(|e| e.to_string())?;

                if additional_offset > 0 {
                    if game_id == GameId::ITC {
                        let start = swap_uint32(files[file_index].toc_entry.start_offset);
                        files[file_index].toc_entry.start_offset =
                            swap_uint32(start + additional_offset);
                    } else {
                        files[file_index].toc_entry.start_offset += additional_offset;
                    }
                }
            }
            continue;
        }

        // process imported files
        let (actual_offset, new_toc_offset) = if game_id == GameId::ITC {
            let ao = (swap_uint32(files[file_index].toc_entry.start_offset) + additional_offset) as u64 * 0x800;
            let no = swap_uint32(files[file_index].toc_entry.start_offset) + additional_offset;
            (ao, no)
        } else {
            let ao = (files[file_index].toc_entry.start_offset + additional_offset) as u64 * 0x800;
            let no = files[file_index].toc_entry.start_offset + additional_offset;
            (ao, no)
        };

        log_printf(LogType::Info, &format!(
            "rebuild_GUTArchive: Block size: {}", files[file_index].block_size
        ));

        let mut input_file = fs::File::open(&files[file_index].infilename)
            .map_err(|e| format!("Failed to open input file: {}", e))?;

        input_file.seek(SeekFrom::End(0)).map_err(|e| e.to_string())?;
        let new_decompressed_size = input_file.stream_position().map_err(|e| e.to_string())? as u32;
        input_file.seek(SeekFrom::Start(0)).map_err(|e| e.to_string())?;

        if !files[file_index].compressed {
            new_dat
                .seek(SeekFrom::Start(actual_offset))
                .map_err(|e| e.to_string())?;

            let mut new_additional_offset = additional_offset;

            let padded_length = ((new_decompressed_size + 0x7FF) / 0x800).max(1);

            if game_id == GameId::ITC {
                while (new_toc_offset + padded_length) * 0x800
                    > (swap_uint32(files[file_index].toc_entry.start_offset)
                        + swap_uint32(files[file_index].toc_entry.end_offset)
                        + new_additional_offset)
                        * 0x800
                {
                    log_printf(LogType::Warning, &format!(
                        "rebuild_GUTArchive: Adjusting additional offset for file index {}",
                        file_index
                    ));
                    new_additional_offset += 1;
                }
            } else {
                while (new_toc_offset + padded_length) * 0x800
                    > (files[file_index].toc_entry.start_offset
                        + files[file_index].toc_entry.end_offset
                        + new_additional_offset)
                        * 0x800
                {
                    log_printf(LogType::Warning, &format!(
                        "rebuild_GUTArchive: Adjusting additional offset for file index {}",
                        file_index
                    ));
                    new_additional_offset += 1;
                }
            }

            let buf_size = padded_length as usize * 0x800;
            let mut uncompressed_data = vec![0u8; buf_size];
            let n = xread(&mut input_file, &mut uncompressed_data, true).map_err(|e| e.to_string())?;
            uncompressed_data.resize(buf_size, 0);
            if n < new_decompressed_size as usize {
                // pad with zeros beyond file content
            }
            xwrite(&mut new_dat, &uncompressed_data).map_err(|e| e.to_string())?;
            drop(input_file);

            if game_id == GameId::ITC {
                let start = swap_uint32(files[file_index].toc_entry.start_offset);
                files[file_index].toc_entry.start_offset =
                    swap_uint32(start + additional_offset);
            } else {
                files[file_index].toc_entry.start_offset += additional_offset;
            }
            files[file_index].toc_entry.compressed_size = new_decompressed_size;
            files[file_index].toc_entry.end_offset = padded_length;
            additional_offset = new_additional_offset;
        } else {
            // compressed
            let mut temp_compressed = std::io::Cursor::new(Vec::new());
            let result = do_compress(
                &mut input_file,
                &mut temp_compressed,
                0x2b,
                7,
                files[file_index].block_size,
            );
            result.map_err(|e| format!("Failed to compress file index {}: {}", file_index, e))?;

            let new_compressed_size = temp_compressed.stream_position().map_err(|e| e.to_string())? as u32;
            temp_compressed.seek(SeekFrom::Start(0)).map_err(|e| e.to_string())?;

            let mut new_additional_offset = additional_offset;
            let padded_length = ((new_compressed_size + 0x7FF) / 0x800).max(1);

            if game_id == GameId::ITC {
                while (new_toc_offset + padded_length) * 0x800
                    > (swap_uint32(files[file_index].toc_entry.start_offset)
                        + swap_uint32(files[file_index].toc_entry.end_offset)
                        + new_additional_offset)
                        * 0x800
                {
                    new_additional_offset += 1;
                }
            } else {
                while (new_toc_offset + padded_length) * 0x800
                    > (files[file_index].toc_entry.start_offset
                        + files[file_index].toc_entry.end_offset
                        + new_additional_offset)
                        * 0x800
                {
                    new_additional_offset += 1;
                }
            }

            let buf_size = padded_length as usize * 0x800;
            let mut comp_data = vec![0u8; buf_size];
            let n = xread(&mut temp_compressed, &mut comp_data, true).map_err(|e| e.to_string())?;
            comp_data.resize(buf_size, 0);
            if n < new_compressed_size as usize {
                // pad with zeros
            }

            new_dat
                .seek(SeekFrom::Start(actual_offset))
                .map_err(|e| e.to_string())?;
            xwrite(&mut new_dat, &comp_data).map_err(|e| e.to_string())?;
            drop(input_file);

            if game_id == GameId::ITC {
                let start = swap_uint32(files[file_index].toc_entry.start_offset);
                files[file_index].toc_entry.start_offset =
                    swap_uint32(start + additional_offset);
            } else {
                files[file_index].toc_entry.start_offset += additional_offset;
            }
            files[file_index].toc_entry.compressed_size = new_compressed_size;
            files[file_index].toc_entry.decompressed_size = new_decompressed_size;
            files[file_index].toc_entry.end_offset = padded_length;
            additional_offset = new_additional_offset;
        }

        pb.inc(1);
        log_printf(LogType::Info, &format!("rebuild_GUTArchive: Imported entry {}", file_index));
        log_printf(LogType::Info, &format!("  Start Offset: {:08x}", files[file_index].toc_entry.start_offset));
        log_printf(LogType::Info, &format!("  Compressed Size: {:08x}", files[file_index].toc_entry.compressed_size));
        log_printf(LogType::Info, &format!("  Decompressed Size: {:08x}", files[file_index].toc_entry.decompressed_size));
        log_printf(LogType::Info, &format!("  End Offset: {:08x}", files[file_index].toc_entry.end_offset));
    }

    pb.finish_and_clear();

    // write changes to TOC
    write_toc_entries(&mut toc_file, &files)?;
    log_printf(LogType::Info, "rebuild_GUTArchive: Wrote changes to TOC");
    println!("\nGUT Archive rebuilt successfully");
    log_printf(LogType::Info, "rebuild_GUTArchive: GUT Archive rebuilt successfully");

    drop(toc_file);
    drop(dat_file);

    // delete old dat file and rename new dat file
    let _ = fs::remove_file(dat_filename);
    new_dat.sync_all().map_err(|e| format!("Failed to sync new.dat: {}", e))?;
    drop(new_dat);
    fs::rename(&new_dat_path, dat_filename).map_err(|e| format!("Failed to rename new.dat: {}", e))?;
    
    Ok(())
}
