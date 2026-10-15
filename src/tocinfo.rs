//! Read-only inspection and reporting for GUT archive `.TOC` files.

use std::fs;

use crate::archive::{GAME_ID, GameId};
use crate::file_io::swap_uint32;

const SECTOR: u64 = 0x800;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Layout {
    Default,
    Kb3,
    Itc,
}

impl Layout {
    pub fn for_game(id: GameId) -> Layout {
        match id {
            GameId::KB3 => Layout::Kb3,
            GameId::ITC => Layout::Itc,
            _ => Layout::Default,
        }
    }

    pub fn current() -> Layout {
        Layout::for_game(GAME_ID.with(|g| g.get()))
    }

    pub fn name(self) -> &'static str {
        match self {
            Layout::Default => "default",
            Layout::Kb3 => "KB3",
            Layout::Itc => "ITC",
        }
    }

    fn stride(self) -> u32 {
        match self {
            Layout::Default => 20,
            Layout::Kb3 | Layout::Itc => 16,
        }
    }
}

#[derive(Clone, Debug)]
pub struct Entry {
    pub index: u32,
    pub start: u32,
    pub stored: u32,
    pub decomp: u32,
    pub end: Option<u32>,
}

impl Entry {
    pub fn compressed(&self) -> bool {
        self.decomp != 0
    }

    pub fn sectors(&self) -> u64 {
        (self.stored as u64).div_ceil(SECTOR)
    }

    pub fn is_dvd_entry(&self, layout: Layout) -> bool {
        layout == Layout::Kb3 && self.start == 0 && !self.compressed()
    }
}

pub struct Toc {
    pub path: String,
    pub layout: Layout,
    pub count: u32,
    pub stride: u32,
    pub header_count: u32,
    pub stamp: [u8; 12],
    pub entries: Vec<Entry>,
}

impl Toc {
    pub fn load(path: &str) -> Result<Toc, String> {
        let data = fs::read(path).map_err(|e| format!("Failed to read {}: {}", path, e))?;
        if data.len() < 16 {
            return Err(format!(
                "{} is too small to be a TOC ({} bytes)",
                path,
                data.len()
            ));
        }

        let layout = Layout::current();
        let stride = layout.stride();
        let body = (data.len() - 16) as u64;

        if !body.is_multiple_of(stride as u64) {
            return Err(format!(
                "{} is not a valid {} TOC: {} - 16 = {} is not a multiple of \
                 the {} byte entry size",
                path,
                layout.name(),
                data.len(),
                body,
                stride
            ));
        }

        let count = (body / stride as u64) as u32;
        if count == 0 {
            return Err(format!("{} contains no entries", path));
        }

        let mut header_count = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
        if layout == Layout::Itc {
            header_count = swap_uint32(header_count);
        }

        let mut stamp = [0u8; 12];
        stamp.copy_from_slice(&data[4..16]);

        let mut entries = Vec::with_capacity(count as usize);
        for i in 0..count {
            let o = 16 + i as usize * stride as usize;
            let rd = |off: usize| -> u32 {
                let p = o + off;
                u32::from_le_bytes([data[p], data[p + 1], data[p + 2], data[p + 3]])
            };

            let entry = match layout {
                Layout::Default => Entry {
                    index: i,
                    start: rd(0),
                    stored: rd(4),
                    end: Some(rd(8)),
                    decomp: rd(16),
                },
                Layout::Kb3 => Entry {
                    index: i,
                    start: rd(0),
                    stored: rd(4),
                    decomp: rd(8),
                    end: None,
                },
                Layout::Itc => Entry {
                    index: i,
                    start: rd(0),
                    stored: swap_uint32(rd(4)),
                    decomp: swap_uint32(rd(8)),
                    end: None,
                },
            };
            entries.push(entry);
        }

        Ok(Toc {
            path: path.to_string(),
            layout,
            count,
            stride,
            header_count,
            stamp,
            entries,
        })
    }

    pub fn next_start(&self, index: usize) -> Option<u32> {
        let next = self.entries.get(index + 1)?;
        if next.start < self.entries[index].start {
            None
        } else {
            Some(next.start)
        }
    }
}

fn human(bytes: u64) -> String {
    const UNITS: [&str; 5] = ["B", "KB", "MB", "GB", "TB"];
    let mut v = bytes as f64;
    let mut u = 0;
    while v >= 1024.0 && u + 1 < UNITS.len() {
        v /= 1024.0;
        u += 1;
    }
    if u == 0 {
        format!("{} {}", bytes, UNITS[0])
    } else {
        format!("{:.2} {}", v, UNITS[u])
    }
}

fn hex(v: u32) -> String {
    format!("0x{:08x}", v)
}

// ---------------------------------------------------------------------------
// -ti
// ---------------------------------------------------------------------------

pub fn print_overview(toc: &Toc) {
    println!("{}", toc.path);
    println!(
        "  layout    {} - {} byte entries",
        toc.layout.name(),
        toc.stride
    );
    println!(
        "  size      {} bytes   (16 header + {} x {})",
        16 + toc.count as u64 * toc.stride as u64,
        toc.count,
        toc.stride
    );
    println!("  entries   {}", toc.count);
    print!("  stamp     ");
    for b in toc.stamp.chunks(4) {
        print!(
            " {}",
            b.iter().map(|x| format!("{:02x}", x)).collect::<String>()
        );
    }
    println!();

    let comp: Vec<&Entry> = toc.entries.iter().filter(|e| e.compressed()).collect();
    let unc: Vec<&Entry> = toc.entries.iter().filter(|e| !e.compressed()).collect();
    let stored_total: u64 = toc.entries.iter().map(|e| e.stored as u64).sum();
    let decomp_total: u64 = toc.entries.iter().map(|e| e.decomp as u64).sum();

    println!();
    println!("storage");
    println!(
        "  compressed    {:>6}   {} stored -> {} out",
        comp.len(),
        human(comp.iter().map(|e| e.stored as u64).sum()),
        human(comp.iter().map(|e| e.decomp as u64).sum())
    );
    println!(
        "  uncompressed  {:>6}   {}",
        unc.len(),
        human(unc.iter().map(|e| e.stored as u64).sum())
    );
    println!(
        "  total         {:>6}   {} stored, {} decompressed",
        toc.count,
        human(stored_total),
        human(decomp_total)
    );

    let dvd = toc
        .entries
        .iter()
        .filter(|e| e.is_dvd_entry(toc.layout))
        .count();
    if dvd > 0 {
        println!(
            "  of which      {:>6}   DVD-resident, not in BUILD.DAT",
            dvd
        );
    }

    println!();
    println!("checks");
    if toc.header_count != toc.count {
        println!(
            "  header count {} does not match the {} entries the file holds",
            toc.header_count, toc.count
        );
    }

    let mut non_monotonic = 0usize;
    let mut first_bad = None;
    for i in 0..toc.entries.len().saturating_sub(1) {
        if toc.entries[i + 1].start < toc.entries[i].start {
            non_monotonic += 1;
            if first_bad.is_none() {
                first_bad = Some(i);
            }
        }
    }
    if non_monotonic == 0 {
        println!("  start sectors are monotonically non-decreasing");
    } else {
        let plural = if non_monotonic == 1 { "" } else { "s" };
        match first_bad {
            Some(i) => println!(
                "  {} non-monotonic start sector{}, first between entries {} and {}",
                non_monotonic,
                plural,
                i,
                i + 1
            ),
            None => println!("  {} non-monotonic start sector{}", non_monotonic, plural),
        }
    }

    let zero = toc.entries.iter().filter(|e| e.stored == 0).count();
    if zero > 0 {
        println!("  {} entries have a stored size of 0", zero);
    }
}

// ---------------------------------------------------------------------------
// -te
// ---------------------------------------------------------------------------

pub fn print_entry(toc: &Toc, index: usize) -> Result<(), String> {
    let e = toc
        .entries
        .get(index)
        .ok_or_else(|| format!("Entry {} out of range (0..{})", index, toc.count))?;

    println!(
        "entry {} of {}  [{}]",
        e.index,
        toc.count,
        toc.layout.name()
    );
    println!("  start sector    {:>12}  ({})", hex(e.start), e.start);
    println!(
        "  stored size     {:>12}  {}",
        e.stored,
        human(e.stored as u64)
    );
    if e.compressed() {
        println!(
            "  decomp size     {:>12}  {}",
            e.decomp,
            human(e.decomp as u64)
        );
    } else {
        println!("  decomp size     {:>12}  0 (stored uncompressed)", "");
    }

    if let Some(end) = e.end {
        println!("  end_offset      {:>12}  {} sectors", hex(end), end);
    } else {
        println!("  sectors         {:>12}", e.sectors());
    }

    let byte0 = e.start as u64 * SECTOR;
    println!(
        "  byte offset     {:>12}  {} ({} bytes)",
        "",
        human(byte0),
        byte0
    );
    println!(
        "  byte extent     {:>12}  {} bytes",
        "",
        e.sectors() * SECTOR
    );

    if let Some(next) = toc.next_start(index) {
        println!("  next entry      {:>12}  sector {}", "", next);
    }

    if e.is_dvd_entry(toc.layout) {
        println!();
        println!("  note: not in BUILD.DAT. The size is that of a file the game");
        println!("        reads from the DVD, and the archive omits the payload.");
    }

    Ok(())
}

// ---------------------------------------------------------------------------
// -tl
// ---------------------------------------------------------------------------

pub struct Filter {
    pub compressed: Option<bool>,
    pub from: u32,
    pub limit: usize,
}

impl Default for Filter {
    fn default() -> Self {
        Filter {
            compressed: None,
            from: 0,
            limit: 200,
        }
    }
}

fn matches(e: &Entry, f: &Filter) -> bool {
    !f.compressed.is_some_and(|c| e.compressed() != c)
}

pub fn print_listing(toc: &Toc, f: &Filter) {
    let mut shown = 0usize;
    let mut total = 0usize;

    println!(
        "{:>7}  {:>10}  {:>11}  {:>11}  note",
        "entry", "start", "stored", "decomp"
    );

    for e in toc.entries.iter().filter(|e| e.index >= f.from) {
        if !matches(e, f) {
            continue;
        }
        total += 1;
        if shown >= f.limit {
            continue;
        }
        shown += 1;

        let note = if e.is_dvd_entry(toc.layout) {
            "DVD-resident, not in BUILD.DAT"
        } else {
            ""
        };

        println!(
            "{:>7}  {:>10}  {:>11}  {:>11}  {}",
            e.index,
            hex(e.start),
            e.stored,
            e.decomp,
            note
        );
    }

    if total == 0 {
        println!("(no entries matched)");
    } else if total > shown {
        println!(
            "... {} more matched (raise --limit to see them, --from to skip ahead)",
            total - shown
        );
    }
}
