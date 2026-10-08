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

/// Top level documentation, printed with `-h`/`--help`.
const ABOUT: &str = "Extract and rebuild the GUT archive format used by Genki games";
const LONG_ABOUT: &str = "\
GUT (most likely short for Genki Utility) Archive is an archive type used by the
video game company Genki, known mostly for their PS2 racing games. This archive
was used in games made around 2003-2006.

This program is an attempt to reverse engineer the archive to allow file modding.";

/// Shared documentation for the trailing free-form flag list.
const AFTER_LONG_HELP: &str = "\
Game switches (only use the ones listed):
  -0    Tokyo Xtreme Racer DRIFT 2, Kaido Racer 2, Kaidou Battle - Touge no
        Densetsu, Wangan Midnight Portable, Ninkyouden
  -2    Import Tuner Challenge, Shutokou Battle X
  -3    Kaidou Battle 1 Taikenban
  -4    Kaidou Battle 2 PurePure 2 Volume 10 Demo

Other flags:
  -log                    Write a log file for the operation (-r, -d, -cd, -cdr, -cb)
  -expanddat              Recursively unpack nested .dat containers into folders
                          like 00000012.dat/ (-d, -cd, -cdr)
  -forcecompressed        Force every imported file to be stored compressed (-r)
  -forceuncompressed      Force every imported file to be stored uncompressed (-r)

See README.md for the list of supported games and their game switches.";

#[derive(Parser)]
#[command(
    name = "gut-archive-tools",
    version,
    about = ABOUT,
    long_about = LONG_ABOUT,
    after_help = "Run 'gut-archive-tools <MODE> --help' for details about a mode.",
    after_long_help = AFTER_LONG_HELP,
    disable_help_subcommand = true
)]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(clap::Subcommand)]
enum Command {
    /// Rebuild files from IN_DIR into BUILD.DAT
    ///
    /// Reads the file list from BUILD.TOC, replaces the entries whose files are
    /// found under IN_DIR with the contents of those files, and writes the
    /// result back into BUILD.DAT. Files in IN_DIR that are not listed in the
    /// TOC are ignored, and TOC entries with no matching file are kept as-is.
    ///
    /// Use this after running -d, editing the extracted files, and putting them
    /// back in the same relative layout.
    ///
    /// The compression state of every imported file is taken from the TOC by
    /// default; use -forcecompressed or -forceuncompressed to override it.
    #[command(name = "-r", after_long_help = AFTER_LONG_HELP)]
    Rebuild {
        /// Path of the .toc file listing the archive contents (e.g. BUILD.TOC)
        #[arg(value_name = "BUILD.TOC")]
        toc: String,

        /// Path of the .dat archive to rebuild; it is modified in place
        #[arg(value_name = "BUILD.DAT")]
        dat: String,

        /// Directory holding the modified files, using the paths from the TOC
        #[arg(value_name = "IN_DIR")]
        in_dir: String,

        /// Game switches and flags, e.g. -0 -log -forcecompressed
        #[arg(value_name = "FLAG", trailing_var_arg = true, allow_hyphen_values = true)]
        extras: Vec<String>,
    },

    /// Decompress and output the archive to OUT_DIR
    ///
    /// Uses BUILD.TOC to locate and decompress every file stored in BUILD.DAT.
    /// Nested .dat containers are written out as plain files; pass -expanddat to
    /// unpack them into folders.
    #[command(name = "-d", after_long_help = AFTER_LONG_HELP)]
    Decompress {
        /// Path of the .toc file listing the archive contents (e.g. BUILD.TOC)
        #[arg(value_name = "BUILD.TOC")]
        toc: String,

        /// Path of the .dat archive to read
        #[arg(value_name = "BUILD.DAT")]
        dat: String,

        /// Directory to create the extracted file tree in
        #[arg(value_name = "OUT_DIR")]
        out_dir: String,

        /// Game switches and flags, e.g. -0 -log -expanddat
        #[arg(value_name = "FLAG", trailing_var_arg = true, allow_hyphen_values = true)]
        extras: Vec<String>,
    },

    /// Extract files from a .dat container
    ///
    /// Extracts every entry of a single .dat container, without needing a .toc.
    /// Nested .dat containers are written out as plain files; pass -expanddat to
    /// unpack them into folders.
    #[command(name = "-cd", after_long_help = AFTER_LONG_HELP)]
    ExtractDat {
        /// Path of the .dat container to extract (e.g. 00000010.DAT)
        #[arg(value_name = "FILE.DAT")]
        dat: String,

        /// Directory to create the extracted file tree in
        #[arg(value_name = "OUT_DIR")]
        out_dir: String,

        /// Game switches and flags, e.g. -0 -log -expanddat
        #[arg(value_name = "FLAG", trailing_var_arg = true, allow_hyphen_values = true)]
        extras: Vec<String>,
    },

    /// Extract a .dat range and collect files matching EXT sequentially
    ///
    /// Walks the .dat containers named 00000000.dat, 00000001.dat, ... found in
    /// IN_DIR, extracting those between START and END (inclusive) and collecting
    /// every file whose extension matches EXT into a single OUT_DIR. This is
    /// mainly used to pull models (.xmdl) out of a game in one go.
    #[command(name = "-cdr", after_long_help = AFTER_LONG_HELP)]
    ExtractDatRange {
        /// Directory holding the numbered .dat containers to read
        #[arg(value_name = "IN_DIR")]
        in_dir: String,

        /// First container index to process, inclusive
        #[arg(value_name = "START")]
        start: u32,

        /// Last container index to process, inclusive (must not be below START)
        #[arg(value_name = "END")]
        end: u32,

        /// Directory to create the collected files in
        #[arg(value_name = "OUT_DIR")]
        out_dir: String,

        /// Extension of the files to collect, without the dot (default: xmdl)
        #[arg(value_name = "EXT")]
        ext: Option<String>,

        /// Game switches and flags, e.g. -0 -log -expanddat
        #[arg(value_name = "FLAG", trailing_var_arg = true, allow_hyphen_values = true)]
        extras: Vec<String>,
    },

    /// Build files into a new .dat container
    ///
    /// Creates a single .dat container out of the entries of IN_DIR. Each entry
    /// must be named with an index prefix followed by an extension (e.g.
    /// 0000.model); files without such a prefix are ignored, and entries are
    /// packed in ascending index order. Contents are stored uncompressed.
    #[command(name = "-cb", after_long_help = AFTER_LONG_HELP)]
    BuildDat {
        /// Directory holding the files to pack into the container
        #[arg(value_name = "IN_DIR")]
        in_dir: String,

        /// Path of the .dat container to create (e.g. 00000010.DAT)
        #[arg(value_name = "OUT_FILE.DAT")]
        out_file: String,

        /// Game switches and flags, e.g. -0 -log
        #[arg(value_name = "FLAG", trailing_var_arg = true, allow_hyphen_values = true)]
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
