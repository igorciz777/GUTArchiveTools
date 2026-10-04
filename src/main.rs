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
use datafile::{DatExpansion, build_datafile, extract_datafile, extract_datafile_range};
use log::{LogType, close_log, log_printf, set_logs};

/// Flag that recursively expands .dat containers into folders.
const EXPAND_DAT_FLAG: &str = "-expanddat";
/// Flag that forces every imported file to be stored compressed.
const FORCE_COMPRESSED_FLAG: &str = "-forcecompressed";
/// Flag that forces every imported file to be stored uncompressed.
const FORCE_UNCOMPRESSED_FLAG: &str = "-forceuncompressed";

use archive::ForceCompression;

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
    ///
    /// Pass -forcecompressed or -forceuncompressed to override the compression
    /// state recorded in the TOC for every imported file.
    #[command(name = "-r")]
    Rebuild {
        toc: String,
        dat: String,
        in_dir: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Decompress and output the archive to OUT_DIR
    ///
    /// .dat containers are written out as plain files. Pass -expanddat to
    /// unpack them into folders.
    #[command(name = "-d")]
    Decompress {
        toc: String,
        dat: String,
        out_dir: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Extract files from a .dat container
    #[command(name = "-cd")]
    ExtractDat {
        dat: String,
        out_dir: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Extract a .dat range and collect files matching EXT sequentially (default: xmdl)
    #[command(name = "-cdr")]
    ExtractDatRange {
        in_dir: String,
        start: u32,
        end: u32,
        out_dir: String,
        ext: Option<String>,
        #[arg(last = true)]
        extras: Vec<String>,
    },
    /// Build files into a new .dat container
    #[command(name = "-cb")]
    BuildDat {
        in_dir: String,
        out_file: String,
        #[arg(last = true)]
        extras: Vec<String>,
    },
}

struct Extras {
    dat_expansion: DatExpansion,
    force_compression: ForceCompression,
}

fn parse_extras(extras: &[String]) -> Result<Extras, String> {
    let mut parsed = Extras {
        dat_expansion: DatExpansion::default(),
        force_compression: ForceCompression::default(),
    };

    for arg in extras {
        if arg == "-log" {
            set_logs(true);
            log_printf(LogType::Info, "Logging enabled");
        } else if arg == EXPAND_DAT_FLAG {
            parsed.dat_expansion = DatExpansion::Expand;
            log_printf(
                LogType::Info,
                "parse_extras: .dat containers will be expanded into folders",
            );
        } else if arg == FORCE_COMPRESSED_FLAG || arg == FORCE_UNCOMPRESSED_FLAG {
            let forced = if arg == FORCE_COMPRESSED_FLAG {
                ForceCompression::Compressed
            } else {
                ForceCompression::Uncompressed
            };

            if parsed.force_compression != ForceCompression::Keep
                && parsed.force_compression != forced
            {
                return Err(format!(
                    "{} and {} are mutually exclusive",
                    FORCE_COMPRESSED_FLAG, FORCE_UNCOMPRESSED_FLAG
                ));
            }

            parsed.force_compression = forced;
            log_printf(
                LogType::Info,
                &format!(
                    "parse_extras: forcing imported files to be {}",
                    match forced {
                        ForceCompression::Compressed => "compressed",
                        _ => "uncompressed",
                    }
                ),
            );
        } else if arg.starts_with('-') && arg.len() == 2 {
            let digits: String = arg.chars().skip(1).collect();
            if digits.chars().all(|c| c.is_ascii_digit()) {
                set_game_id(arg);
            }
        }
    }

    Ok(parsed)
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
            let extras = parse_extras(&extras)?;
            rebuild_gut_archive(&toc, &dat, &in_dir, extras.force_compression)?;
        }
        Command::Decompress {
            toc,
            dat,
            out_dir,
            extras,
        } => {
            let extras = parse_extras(&extras)?;
            let mut toc_file =
                File::open(&toc).map_err(|e| format!("Failed to open .toc file: {}", e))?;
            let mut dat_file =
                File::open(&dat).map_err(|e| format!("Failed to open .dat file: {}", e))?;
            extract_gut_archive_all(&mut toc_file, &mut dat_file, &out_dir, extras.dat_expansion)?;
        }
        Command::ExtractDat {
            dat,
            out_dir,
            extras,
        } => {
            let extras = parse_extras(&extras)?;
            extract_datafile(std::path::Path::new(&dat), &out_dir, extras.dat_expansion)?;
        }
        Command::ExtractDatRange {
            in_dir,
            start,
            end,
            out_dir,
            ext,
            extras,
        } => {
            if start > end {
                return Err("Invalid range for -cdr".into());
            }
            let extras = parse_extras(&extras)?;
            extract_datafile_range(
                &in_dir,
                start,
                end,
                &out_dir,
                &ext.unwrap_or("xmdl".into()),
                extras.dat_expansion,
            )?;
        }
        Command::BuildDat {
            in_dir,
            out_file,
            extras,
        } => {
            parse_extras(&extras)?;
            build_datafile(&in_dir, &out_file)?;
        }
    }

    close_log();
    Ok(())
}
