mod archive;
mod datafile;
mod file_io;
mod headers;
mod log;
mod ucl;
mod ucl_ffi;

use std::fs::File;

use clap::Parser;

use archive::{extract_gut_archive_all, rebuild_gut_archive, set_game_id};
use log::{close_log, log_printf, set_logs, LogType};

#[derive(Parser)]
#[command(
    name = "gut-archive-tools",
    version,
    about = "GUT Archive Tools - extract and rebuild GUT archive format"
)]
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
            extras,
        } => {
            parse_extras(&extras);
            let mut toc_file =
                File::open(&toc).map_err(|e| format!("Failed to open .toc file: {}", e))?;
            let mut dat_file =
                File::open(&dat).map_err(|e| format!("Failed to open .dat file: {}", e))?;
            extract_gut_archive_all(&mut toc_file, &mut dat_file, &out_dir)?;
        }
    }

    close_log();
    Ok(())
}
