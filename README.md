# GUT Archive Tools
GUT Archive is an archive type used by the video game company Genki, known mostly for their PS2 racing games.
This archive was used in games made around 2003-2006.

This program is an attempt to reverse engineer the archive to allow file modding

## Usage
```shell
gut_archive.exe/.out [-r tocFile datFile inDirectory] [-d tocFile datFile outputDirectory] -0,...
```
### Options
- -r: Rebuild files in inDirectory into datFile
- -d: Decompress and output the archive to outputDirectory
- -0,...: Switch between compatible games (optional, use only if stated in compatible game list)
    - -0: Tokyo Xtreme Racer DRIFT 2

### Examples
```shell
.\gut_archive.exe -d .\BUILD.TOC .\BUILD.DAT BUILD_OUT
```
```shell
.\gut_archive.exe -d .\BUILD.TOC .\BUILD.DAT BUILD_OUT -0
```


## Known compatible games
- RACING BATTLE -C1 GRAND PRIX- (SLPM-65897) 
- Fu-un Bakumatsu-den (SLPM-61096)
- Tokyo Xtreme Racer 3 (SLUS-20831)

## Known incompatible games
- Tokyo Xtreme Racer DRIFT 2 (SLUS-21394) - toc offsets get weirdly mixed up towards the end, use -0 to extract some available files

## Current issues
- Everything is saved into a generic file type
- Recompression not implemented yet
- Lacks proper error handling in many places
- Lacks options regarding creating a .log
- Possible issues with building on both platforms

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