# lfs_macos

This is an implementation of a third-party [log structured file system](https://web.stanford.edu/~ouster/cgi-bin/papers/lfs.pdf) for macos. 

Requires [osxfuse](https://osxfuse.github.io/).

## Building LFS

Call the build script 

`./build.sh`

## Starting LFS

After building, to start the LFS:

`lfs [options] file mountpoint`

Where the options consist of:

`-f`

Pass the ‘-f’ argument to fuse_main so it runs in the foreground.

`-s num` or `--cache=num`

Size of the cache in the Log layer, in segments. Default is 4.

`-i num` or `--interval=num`

Checkpoint interval, in segments. Default is 4.

`-c num` or `--start=num`

Threshold at which cleaning starts, in segments. Default is 4.

`-C num` or `--stop=num`

Threshold at which cleaning stops, in segments. Default is 8.

The file argument specifies the name of the virtual flash file, and mountpoint specifies the
directory on which the LFS filesystem should be mounted.

## Layers and design

The implementation uses the following hierarchical structure:

### 1. Flash Layer

A layer which emulates flash memory. Contained in flash.c and flash.h.

### 2. Log Layer

Creates and maintains the log that is stored on flash. Log_Write and Log_Read are one file block at a time. Contained in layers/log.hpp. For checkpointing and checkpoint recovery, there is a reserved segment which checkpoints are written to circularly for wear leveling. On recovery, the log layer iterates through the reserved segment and finds the most recent checkpoint.

### 3. File Layer

Implements the file abstraction and does cleaning. Contained in layers/file.hpp. Cleaning makes use of Log and File layer functions.

### 4. Directory Layer

Implements the directory hierarchy and supplies the FUSE layer with higher level file functions. Contained in layers/directory.hpp

### 5. FUSE Layer

The fuse layer is found in layers/fuse.hpp. It contains the functions called directly by the fuse handlers found in fuse_handlers.hpp. The fuse layer is thin but it has a bit of fuse logic which is kept out of the directory layer. 

## Utilities

### mklfs

The mklfs utility creates and formats the flash for LFS.

mklfs writes the initial file system which consists of the '.', '..', and ‘.ifile’ entries in the root directory. To call mklfs:

`mklfs [options] file`

where file is the name of the virtual flash file to create and the options consist of:

`-b size` or `--block=size`

Size of a block, in sectors. The default is 2 (1KB).

`-l size` or `--segment=size`

Segment size, in blocks. The segment size must be a multiple of the flash
erase block size, report an error otherwise. The default is 32.

`-s segments` or `--segments=segments`

Size of the flash, in segments. The default is 100.

`-w limit` or `--wearlimit=limit`

Wear limit for erase blocks. The default is 1000.

### lfsck
The lfsck utlity that reads the metadata and data from the flash and checks for the following errors:
- in-use inodes that do not have directory entries
- directory entries that refer to unused inodes
- incorrect segment summary information

USAGE:

`lfsck file`

where file is the name of the virtual flash file to check.
