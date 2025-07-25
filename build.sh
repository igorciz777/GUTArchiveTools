#! /bin/sh
gcc -Wall -Wextra -Wpedantic -O2 -fomit-frame-pointer src/main.c -o gut_archive.out -Iinclude -I. src/lib/x86_64-linux-gnu/libucl.a