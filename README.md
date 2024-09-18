# GUT Archive Tools
GUT Archive is an archive type used by the video game company Genki, known mostly for their PS2 racing games.
This archive was used in games made around 2003-2006.

This program is an attempt to reverse engineer the archive to allow file modding.

## Usage
```shell
gut_archive [mode] -0,... [log]
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
- **-0**: Tokyo Xtreme Racer DRIFT 2, Kaido Racer 2, Kaidou Battle - Touge no Densetsu
- **-1**: Tokyo Xtreme Racer 3, Shutokou Battle 01
- **-2**: Import Tuner Challenge, Shutokou Battle X

### Logs
- **-log**: Save a log file after decompression/rebuilding

### Examples
Regular decompression and extraction
```shell
.\gut_archive.exe -d .\BUILD.TOC .\BUILD.DAT BUILD_OUT
```
Decompression with logging
```shell
.\gut_archive.exe -d .\BUILD.TOC .\BUILD.DAT BUILD_OUT -log
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

## Game compatibility table

:heavy_check_mark:  - Works fine

:question: - Not tested

:x: - Doesn't work

:heavy_minus_sign: - Unrelated to the game

| **Name**                                     | **Serial** | **System** | **Extract**        | **Rebuild**        | **Reimport**       | **Comp. flag** |
|:--------------------------------------------:|:----------:|:----------:|:------------------:|:------------------:|:------------------:|:--------------:|
| Fu-un Bakumatsu-den                          | SLPM 61096 | PS2        | :question:         | :x:                | :heavy_check_mark: |                |
| Fu-un Shinsengumi                            | SLKA 25139 | PS2        | :question:         | :question:         | :question:         |                |
| Import Tuner Challenge                       | SLUS 21394 | X360       | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: | -2             |
| Kaido Racer                                  | SLES 53191 | PS2        | :heavy_check_mark: | :question:         | :question:         |                |
| Kaido Racer 2                                | SLES 53900 | PS2        | :heavy_check_mark: | :x:                | :heavy_check_mark: | -0             |
| Kaidou Battle - Nikko, Haruna, Rokko, Hakone | SLPM 65246 | PS2        | :heavy_check_mark: | :question:         | :question:         |                |
| Kaidou Battle - Touge no Densetsu            | SLPM 61121 | PS2        | :heavy_check_mark: | :x:                | :heavy_check_mark: | -0             |
| Kaidou Battle 1 Taikenban Demo               | SLPM 60195 | PS2        | :heavy_check_mark: | :x:                | :question:         | -3             |
| Kaidou Battle 2 - Chain Reaction             | SLPM 60228 | PS2        | :x:                | :x:                | :x:                |                |
| Ninkyouden: Toseinin Ichidaiki               | SLPM 61144 | PS2        | :question:         | :x:                | :question:         | -0             |
| Racing Battle: C1 Grand Prix                 | SLPM 65897 | PS2        | :heavy_check_mark: | :heavy_check_mark: | :heavy_minus_sign: |                |
| Racing Battle: C1 Grand Prix Taikenban       | SLPM 61115 | PS2        | :heavy_check_mark: | :question:         | :question:         |                |
| Shutokou Battle 01                           | SLPM 65308 | PS2        | :heavy_check_mark: | :question:         | :question:         | -1             |
| Street Supremacy                             | ULUS 10069 | PSP        | :heavy_check_mark: | :question:         | :question:         |                |
| Tokyo Xtreme Racer 3                         | SLUS 20831 | PS2        | :heavy_check_mark: | :question:         | :question:         | -1             |
| Tokyo Xtreme Racer Drift                     | SLUS 21236 | PS2        | :heavy_check_mark: | :question:         | :question:         |                |
| Tokyo Xtreme Racer DRIFT 2                   | SLUS 21394 | PS2        | :heavy_check_mark: | :x:                | :heavy_check_mark: | -0             |
| Wangan Midnight Portable                     | ULJM 05264 | PSP        | :heavy_check_mark: | :x:                | :question:         | -0             |



## Current issues
- Very few file types are defined, none for xbox 
- Problems with many games
- Lacks proper error handling in many places
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
- [**UCL**](https://www.oberhumer.com/opensource/ucl/) - Used for compression and decompression.
- [**dirent for Windows**](https://github.com/tronkko/dirent) - Used for directory listing on Windows.
