# LFS Project

This implementation is in C++ using osxfuse. All of the required features and fuse handlers are implemented. Roll forward is not. The design mostly follows the suggested hierarchical design in the spec, with some small deviations which are noted below. Take a look at the interfaces at the top of each file in in the layers/ directory for an overview of each layers functionality.

## DISCLAIMER: Works on MacOS with osxfuse found here https://osxfuse.github.io/. Please use on macOS. 

## Building LFS

Call the build script 

`./build.sh`

This will build the flash layer, along with all of our layers, tests, and lfs_main which starts fuse.

## Calling LFS from the command line

After building, to start the LFS from the command line use the following command:

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

The implementation is in C++ and uses the following hierarchical structure:

### 1. Flash Layer

A layer which emulates flash memory. Contained in flash.c and flash.h. Provided by instructor.

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

mklfs writes the initial file system which consists of the '.', '..', and ‘.ifile’ entries in the root directory. To call mklfs, use the following bash command:

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
The lfsck utlity is written in C++. It reads the metadata and data from the flash and checks for the following errors:
- in-use inodes that do not have directory entries
- directory entries that refer to unused inodes
- incorrect segment summary information
- and possibly more for phase 2

USAGE:

`lfsck file`

where file is the name of the virtual flash file to check.

## other

Data structures such as inode, segment summary, segment cache, etc.. are all in the data_structures folder. Unit tests are in the tests folder.
