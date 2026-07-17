mod archive;
mod datafile;
mod file_io;
mod headers;
mod log;
mod ucl;
mod ucl_ffi;

use std::fs::File;

use clap::Parser;

use archive::{
    extract_gut_archive_all, rebuild_gut_archive, set_game_id,
};
use datafile::{build_datafile, extract_datafile, extract_datafile_range};
use log::{close_log, log_printf, set_logs, LogType};

#[derive(Parser)]
#[command(name = "gut-archive-tools", version, about = "GUT Archive Tools - extract and rebuild Genki game archives")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(clap::Subcommand)]
enum Command {
    /// Rebuild files from IN_DIR into BUILD.DAT
    #[command(name = "-r")]
    Rebuild {
        toc: String,
        dat: String,
        in_dir: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Decompress and output the archive to OUT_DIR
    #[command(name = "-d")]
    Decompress {
        toc: String,
        dat: String,
        out_dir: String,
        /// Recursively extract .dat datafiles into subdirectories
        #[arg(short = 'r', long = "extract-dats")]
        extract_dats: bool,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Extract files from a .dat container
    #[command(name = "-cd")]
    ExtractData {
        dat: String,
        out_dir: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Extract .dat range, collect .EXT files sequentially
    #[command(name = "-cdr")]
    ExtractDataRange {
        in_dir: String,
        start: u32,
        end: u32,
        out_dir: String,
        #[arg(default_value = "xmdl")]
        ext: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Build files into a new .dat container
    #[command(name = "-cb")]
    BuildData {
        in_dir: String,
        out_file: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
}

fn parse_extras(extras: &[String]) {
    for arg in extras {
        if arg == "-log" {
            set_logs(true);
            log_printf(LogType::Info, "Logging enabled");
        } else if arg.starts_with('-') && arg.len() >= 2 && arg.len() <= 3 {
            let digits: String = arg.chars().skip(1).collect();
            if digits.len() == 1 && digits.chars().all(|c| c.is_ascii_digit()) {
                set_game_id(arg);
            }
        }
    }
}

fn main() -> Result<(), String> {
    let cli = Cli::parse();

    let result = ucl_ffi::ucl_init();
    if result != 0 {
        return Err("ucl_init() failed".into());
    }

    match cli.command {
        Command::Rebuild {
            toc,
            dat,
            in_dir,
            extras,
        } => {
            parse_extras(&extras);
            rebuild_gut_archive(&toc, &dat, &in_dir)?;
        }
        Command::Decompress {
            toc,
            dat,
            out_dir,
            extract_dats,
            extras,
        } => {
            parse_extras(&extras);
            let mut toc_file =
                File::open(&toc).map_err(|e| format!("Failed to open .toc file: {}", e))?;
            let mut dat_file =
                File::open(&dat).map_err(|e| format!("Failed to open .dat file: {}", e))?;
            extract_gut_archive_all(&mut toc_file, &mut dat_file, &out_dir, extract_dats)?;
        }
        Command::ExtractData { dat, out_dir, extras } => {
            parse_extras(&extras);
            let datafile =
                File::open(&dat).map_err(|e| format!("Failed to open .dat file: {}", e))?;
            extract_datafile(datafile, &std::path::Path::new(&out_dir))?;
        }
        Command::ExtractDataRange {
            in_dir,
            start,
            end,
            out_dir,
            ext,
            extras,
        } => {
            parse_extras(&extras);
            extract_datafile_range(&in_dir, start, end, &out_dir, &ext)?;
        }
        Command::BuildData {
            in_dir,
            out_file,
            extras,
        } => {
            parse_extras(&extras);
            build_datafile(&in_dir, &out_file)?;
        }
    }

    close_log();
    Ok(())
}
