# GUT Archive Tools
GUT Archive is an archive type used by the video game company Genki, known mostly for their PS2 racing games.
This archive was used in games made around 2003-2006.

This program is an attempt to reverse engineer the archive to allow file modding.

## Usage
```shell
gut_archive [mode] -0,...
```
### Modes
- **-r** <BUILD.TOC> <BUILD.DAT> <IN_DIR>: Rebuild files in <IN_DIR> into <BUILD.DAT>
- **-d** <BUILD.TOC> <BUILD.DAT> <OUT_DIR>: Decompress and output the archive to <OUT_DIR>
- **-dr** <BUILD.TOC> <BUILD.DAT> <OUT_DIR>: Decompress and output the archive recursively (any .dat files inside) to <OUT_DIR>
- **-rr** <BUILD.TOC> <BUILD.DAT> <IN_DIR>: Rebuild files in <IN_DIR> recursively, <DAT_OUT> needs to have same name as the .dat file without extension
- **-cd** <FILE.DAT> <OUT_DIR>: Extract files from a .dat container (different from BUILD.DAT!!!)
- **-cr** <FILE.DAT> <IN_DIR>: Rebuild files into a .dat container (different from BUILD.DAT!!!)
- **-l** <CDDATA.LOC> <CDDATA.000> <OUT_DIR>: Legacy mode for older games

### Game switches
- **-0**: Tokyo Xtreme Racer DRIFT 2
- **-1**: Tokyo Xtreme Racer 3, Shutokou Battle 01
- **-2**: Import Tuner Challenge

### Examples
Regular decompression and extraction
```shell
.\gut_archive.exe -d .\BUILD.TOC .\BUILD.DAT BUILD_OUT
```
For TXR:D2 and alike
```shell
.\gut_archive.exe -d .\BUILD.TOC .\BUILD.DAT BUILD_OUT -0
```
Extracting a .dat container
```shell
.\gut_archive.exe -cd .\00000010.DAT DAT_OUT
```
Rebuilding a .dat container
```shell
.\gut_archive.exe -cr .\00000010.DAT DAT_IN
```
Extracting BUILD.DAT recursively
```shell
.\gut_archive.exe -dr .\BUILD.TOC .\BUILD.DAT BUILD_OUT
```

## Compatible games
- Kaido Racer (SLES-53191)
- Kaidou Battle - Nikko, Haruna, Rokko, Hakone (SLPM-65246) (SLKA-25063)
- Fu-un Bakumatsu-den (SLPM-61096)
- RACING BATTLE -C1 GRAND PRIX- (SLPM-65897) (SLPM-61115)
- Street Supremacy (ULUS-10069) - no file types defined yet
- Shutokou Battle 01 (SLPM-65308) (SLPM-60206), use -1 to skip a duped block of data in file id 1 (only in release version, demo works with default)
- Tokyo Xtreme Racer 3 (SLUS-20831), use -1 to skip a duped block of data in file id 1
- Tokyo Xtreme Racer Drift (SLUS-21236)

## Semi-compatible games
- Tokyo Xtreme Racer DRIFT 2 (SLUS-21394) - toc offsets get weirdly mixed up towards the end, use -0 to extract some available files
- Kaido Racer 2 (SLES-53900) - same as txrd2 but different file count
- Kaidou Battle - Touge no Densetsu (SLPM-61121) - same as txrd2 but different file count
- Ninkyouden: Toseinin Ichidaiki (SLPM-61144) - similar to txrd2, -0 to extract some files
- Wangan Midnight Portable (ULJM-05264) - similar to txrd2, -0 to extract some files
- Import Tuner Challenge - experimental support with -2, no file types defined yet
- Fu-un Shinsengumi (SLKA-25139) - extracting .dat containers does not work

## Incompatible games
- Kaidou Battle 2 - Chain Reaction (SLPM-60228) - weird misalignment after 10th file in toc, exports files still UCL compressed
- Kaidou Battle 1 Taikenban Demo (SLPM-60195) - very weird toc file contents, does not export anything


## Current issues
- Very few file types are defined, none for xbox and psp
- Problems with many games
- Lacks proper error handling in many places
- Lacks options regarding creating a .log
- Possible issues with building on both platforms
- Code is a spaghetti mess

## Building
### Windows
Tested only with mingw gcc
```shell
gcc -Wall -O2 -fomit-frame-pointer src/gut_archive.c -o gut_archive.exe -Iinclude -I. src/lib/win32/libucl.a
```

### Linux
```shell
gcc -Wall -O2 -fomit-frame-pointer src/gut_archive.c -o gut_archive.out -Iinclude -I. src/lib/x86_64-linux-gnu/libucl.a
```

## Credits
- [**UCL**](https://www.oberhumer.com/opensource/ucl/) - Used for decompression.
- [**dirent for Windows**](https://github.com/tronkko/dirent) - Used for directory listing on Windows.
