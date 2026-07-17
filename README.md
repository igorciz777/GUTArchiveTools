# GUT Archive Tools

GUT (most likely short for Genki Utility) Archive is an archive type used by the video game company Genki, known mostly for their PS2 racing games. This archive was used in games made around 2003-2006.

This program is an attempt to reverse engineer the archive to allow file modding.

## Usage

```
gut-archive-tools [OPTIONS] <MODE> [ARGS]
```

### Modes
- **-r** `<BUILD.TOC> <BUILD.DAT> <IN_DIR>`: Rebuild files in `<IN_DIR>` into `<BUILD.DAT>`
- **-d** `<BUILD.TOC> <BUILD.DAT> <OUT_DIR>`: Decompress and output the archive to `<OUT_DIR>`
- **-cd** `<FILE.DAT> <OUT_DIR>`: Extract files from a .dat container
- **-cb** `<FILE.DAT> <IN_DIR>`: Build a .dat container from files in `<IN_DIR>`
- **-cdr** `<IN_DIR> <START> <END> <OUT_DIR> <EXT>`: Extract a range of .dat containers and collect files with a given extension

### Game switches
- **-0**: Tokyo Xtreme Racer DRIFT 2, Kaido Racer 2, Kaidou Battle - Touge no Densetsu, other games listed [below](#game-compatibility-table)
- **-2**: Import Tuner Challenge, Shutokou Battle X
- **-3**: Kaidou Battle 1 Taikenban
- **-4**: Kaidou Battle 2 PurePure 2 Volume 10 Demo

### Logs
- **-log**: Save a log file after decompression/rebuilding

### Examples

Regular decompression and extraction:
```shell
gut-archive-tools -d BUILD.TOC BUILD.DAT BUILD_OUT
```

With game switch and logging:
```shell
gut-archive-tools -d BUILD.TOC BUILD.DAT BUILD_OUT -0 -log
```

Extracting a .dat container:
```shell
gut-archive-tools -cd 00000010.DAT DAT_OUT
```

Building a .dat container:
```shell
gut-archive-tools -cb 00000010.DAT DAT_IN
```

Rebuild after modifying files:
```shell
gut-archive-tools -r BUILD.TOC BUILD.DAT MODIFIED_FILES
```

## Game compatibility table

:heavy_check_mark:  - Works fine

:question: - Not tested

:x: - Doesn't work

:heavy_minus_sign: - Unrelated to the game

|                     **Name**                    | **Serial** | **System** |     **Extract**    |     **Rebuild**    |    **Reimport**    | **Comp. flag** |
|:-----------------------------------------------:|:----------:|:----------:|:------------------:|:------------------:|:------------------:|:--------------:|
| Fu-un Bakumatsu-den                             | SLPM 65813 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Import Tuner Challenge                          | US-2030    |     X360   | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |       -2       |
| Kaido Racer                                     | SLES 53191 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Kaido Racer 2                                   | SLES 53900 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |       -0       |
| Kaido Battle (Korea)                            | SLKA 25063 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Kaidou Battle - Nikko, Haruna, Rokko, Hakone    | SLPM 65246 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Kaidou Battle 2 - Chain Reaction                | SLPM 65514 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Kaidou Battle - Touge no Densetsu               | SLPM 61121 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |       -0       |
| Ninkyouden: Toseinin Ichidaiki Taikenban        | SLPM 61144 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |       -0       |
| Racing Battle: C1 Grand Prix                    | SLPM 65897 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Racing Battle: C1 Grand Prix Taikenban          | SLPM 61115 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Shutokou Battle 01                              | SLPM 65308 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Shutokou Battle X                               | GE-2001    |     X360   | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |       -2       |
| Street Supremacy                                | ULUS 10069 |     PSP    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Tokyo Xtreme Racer 3                            | SLUS 20831 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Tokyo Xtreme Racer Drift                        | SLUS 21236 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Tokyo Xtreme Racer DRIFT 2                      | SLUS 21394 |     PS2    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |       -0       |
| Wangan Midnight Portable                        | ULJM 05264 |     PSP    | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |       -0       |
| Fu-un Bakumatsu-den Taikenban                   | SLPM 61096 |     PS2    | :heavy_check_mark: |     :question:     |     :question:     |                |
| Fu-un Bakumatsu-den PurePure 2 Vol. 12 Demo     | LPM  61096 |     PS2    | :heavy_check_mark: |     :question:     |     :question:     |                |
| Kaidou Battle 1 Taikenban Demo                  | SLPM 60195 |     PS2    | :heavy_check_mark: |         :x:        |     :question:     |       -3       |
| Kaidou Battle 2 Taikenban                       | SLPM 60228 |     PS2    | :heavy_check_mark: |         :x:        |         :x:        |                |
| Kaidou Battle 2 PurePure 2 Vol. 10 Demo         | LPM  65514 |     PS2    | :heavy_check_mark: |     :question:     | :heavy_minus_sign: |       -4       |
| Shutokou Battle 01 Taikenban                    | SLPM 60206 |     PS2    | :heavy_check_mark: |         :x:        |         :x:        |                |
| Fu-un Shinsengumi                               | SLPM 65494 |     PS2    | :heavy_check_mark: |     :question:     |     :question:     |                |
| Fu-un Shinsengumi (Korea)                       | SLKA 25139 |     PS2    | :heavy_check_mark: |     :question:     |     :question:     |                |
| Fu-un Shinsengumi Taikenban                     | SLPM 60225 |     PS2    | :heavy_check_mark: |     :question:     |     :question:     |                |
| Ninkyouden: Toseinin Ichidaiki                  | SLPM 66274 |     PS2    |     :question:     |     :question:     |     :question:     |                |
| Shutokou Battle: Zone of Control                | ULJM 05017 |     PSP    |     :question:     |     :question:     |     :question:     |                |

## Current issues
- A lot of undefined file types
- Problems with some games

## Building

### Prerequisites
- [Rust](https://www.rust-lang.org/tools/install)
- System `libucl` (UCL compression library)

### Linux
```shell
sudo apt install libucl-dev
cargo build --release
```

The binary will be at `target/release/gut-archive-tools`.

### Windows
Install UCL via MSYS2 UCRT64, then:
```shell
cargo build --release
```

## Credits
- [**UCL**](https://www.oberhumer.com/opensource/ucl/) - Used for compression and decompression.
