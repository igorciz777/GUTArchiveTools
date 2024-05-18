# GUT Archive Tools
GUT Archive is an archive type used by the video game company Genki, known mostly for their PS2 racing games.
This archive was used in games made around 2003-2006.

## Usage
```shell
gut_archive.exe [-d <TOC File> <DAT File> <Output Directory>] [-r <Input Directory> <Output Directory>]
```

## Known compatible games
- RACING BATTLE -C1 GRAND PRIX-

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
