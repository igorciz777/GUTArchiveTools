pub struct FileExtension {
    pub ext: &'static str,
    pub magic: &'static [u8],
}

pub const UCL_MAGIC: &[u8; 8] = b"\x00\xe9UCL\xff\x01\x1a";
pub const XVI: &[u8; 8] = b"0IVX00.1";
pub const TIM2: &[u8; 4] = b"TIM2";
pub const SQ: &[u8; 24] = &[
    0x49, 0x45, 0x43, 0x53, 0x73, 0x72, 0x65, 0x56,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x49, 0x45, 0x43, 0x53, 0x75, 0x71, 0x65, 0x53,
];
pub const HD: &[u8; 24] = &[
    0x49, 0x45, 0x43, 0x53, 0x73, 0x72, 0x65, 0x56,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x49, 0x45, 0x43, 0x53, 0x64, 0x61, 0x65, 0x48,
];
pub const BIN: &[u8; 17] = &[0u8; 17];
pub const BD: &[u8; 16] = &[0u8; 16];
pub const BD_2: &[u8; 16] = b"\x00\x07wwwwwwwwwwwwww";
pub const XPR2: &[u8; 4] = b"XPR2";
pub const TX2D: &[u8; 4] = b"TX2D";
pub const GIM: &[u8; 4] = b"MIG.";
pub const GMO: &[u8; 4] = b"OMG.";
pub const GXT: &[u8; 4] = b"GXT\x00";
pub const GIM_BIG: &[u8; 4] = b".GIM";
pub const GMO_BIG: &[u8; 4] = b".GMO";
pub const GXT_BIG: &[u8; 4] = b"\x00TXG";
pub const XMD_BIG: &[u8; 4] = b"XMD\x00";
pub const XFN_BIG: &[u8; 4] = b"\xff\xaa\xff\xaa";
pub const XPU_BIG: &[u8; 3] = b"\x10*\x0e";
pub const DOC_BIG: &[u8; 4] = b"DOC\x00";
pub const DNBW_BIG: &[u8; 4] = b"DNBW";
pub const KBDS_BIG: &[u8; 4] = b"KBDS";
pub const DDS: &[u8; 4] = b"DDS ";
pub const VAG: &[u8; 4] = b"VAGp";
pub const HIDE: &[u8; 4] = b"EDIH";
pub const GINF: &[u8; 4] = b"FNIG";
pub const PNG: &[u8; 8] = b"\x89PNG\r\n\x1a\n";
pub const JPG: &[u8; 3] = b"\xff\xd8\xff";
pub const BMP: &[u8; 2] = b"BM";
pub const GIF: &[u8; 3] = b"GIF";
pub const TGA: &[u8; 18] = b"TRUEVISION-XFILE.\x00";
pub const BSPR: &[u8; 4] = b"RPSB";
pub const CLT2: &[u8; 4] = b"CLT2";

pub const FILE_EXTENSIONS: &[FileExtension] = &[
    FileExtension { ext: "tm2", magic: TIM2 },
    FileExtension { ext: "sq", magic: SQ },
    FileExtension { ext: "hd", magic: HD },
    FileExtension { ext: "bin", magic: BIN },
    FileExtension { ext: "bd", magic: BD },
    FileExtension { ext: "bd", magic: BD_2 },
    FileExtension { ext: "xpr", magic: XPR2 },
    FileExtension { ext: "txd", magic: TX2D },
    FileExtension { ext: "gim", magic: GIM },
    FileExtension { ext: "gmo", magic: GMO },
    FileExtension { ext: "gxt", magic: GXT },
    FileExtension { ext: "gim", magic: GIM_BIG },
    FileExtension { ext: "gmo", magic: GMO_BIG },
    FileExtension { ext: "gxt", magic: GXT_BIG },
    FileExtension { ext: "xmd", magic: XMD_BIG },
    FileExtension { ext: "xfn", magic: XFN_BIG },
    FileExtension { ext: "xpu", magic: XPU_BIG },
    FileExtension { ext: "doc", magic: DOC_BIG },
    FileExtension { ext: "xwb", magic: DNBW_BIG },
    FileExtension { ext: "xsb", magic: KBDS_BIG },
    FileExtension { ext: "hide", magic: HIDE },
    FileExtension { ext: "dds", magic: DDS },
    FileExtension { ext: "vag", magic: VAG },
    FileExtension { ext: "ginf", magic: GINF },
    FileExtension { ext: "png", magic: PNG },
    FileExtension { ext: "jpg", magic: JPG },
    FileExtension { ext: "bmp", magic: BMP },
    FileExtension { ext: "gif", magic: GIF },
    FileExtension { ext: "tga", magic: TGA },
    FileExtension { ext: "bspr", magic: BSPR },
    FileExtension { ext: "clt", magic: CLT2 },
    FileExtension { ext: "xmdl", magic: XVI },
];

/// Finds file extension by matching header bytes.
pub fn find_file_extension_header(data: &[u8]) -> &'static str {
    for fe in FILE_EXTENSIONS {
        if data.len() >= fe.magic.len() && &data[..fe.magic.len()] == fe.magic {
            return fe.ext;
        }
    }
    "bin"
}

/// Finds file extension by matching footer bytes.
pub fn find_file_extension_footer(data: &[u8]) -> &'static str {
    for fe in FILE_EXTENSIONS {
        if data.len() >= fe.magic.len()
            && &data[data.len() - fe.magic.len()..] == fe.magic
        {
            return fe.ext;
        }
    }
    "bin"
}
