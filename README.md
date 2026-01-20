# File System Defragmenter

## Overview
A disk defragmentation utility for the old Unix file system that reorganizes fragmented files by laying out all blocks sequentially on disk to improve read performance.

## Key Features
- **Block Reorganization**: Rearranges file data blocks to be contiguous on disk
- **Inode Preservation**: Maintains original inode numbers and metadata
- **Indirect Block Handling**: Properly orders direct, single, double, and triple indirect blocks
- **Free List Optimization**: Creates sorted free block list to prevent future fragmentation
- **Binary I/O**: Reads and writes disk images at byte level with proper struct alignment

## Files Modified/Created

### Created Files
- [`defrag.c`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/blob/main/defrag.c) - Complete defragmentation implementation (735 lines)
- [`Makefile`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/blob/main/Makefile) - Build configuration with C11 standard and optimization flags

## How to Build and Run

### Build
```bash
make
```

This creates the `defrag` executable.

### Run
```bash
./defrag <input_disk_image>
```

For example:
```bash
./defrag images_frag/disk_frag_1
```

The program reads the fragmented disk image and writes the defragmented version to a file named `disk_defrag` in the current directory.

### Test
Compare output with expected results:
```bash
diff disk_defrag images_defrag/disk_defrag_1
```

Or use the [diff scripts](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/diff_scripts) for percentage match:
```bash
python diff_scripts/diff.py disk_defrag images_defrag/disk_defrag_1
# or
deno run diff_scripts/diff.ts disk_defrag images_defrag/disk_defrag_1
```

### Clean
```bash
make clean
```

## Disk Structure

The program handles disk images with the following layout:

```
Boot Block      (512 bytes)
Super Block     (512 bytes)
Inode Region    (variable size - array of 100-byte inodes)
Data Region     (variable size - blocksize-aligned data blocks)
Swap Space      (variable size - copied as-is)
```

### Key Data Structures

**Superblock** (24 bytes):
- `blocksize` - Size of data blocks in bytes
- `inode_offset` - Inode region start (in blocks)
- `data_offset` - Data region start (in blocks)
- `swap_offset` - Swap space start (in blocks)
- `free_inode` - Head of free inode list
- `free_block` - Head of free block list

**Inode** (100 bytes):
- 10 direct block pointers (`dblocks[10]`)
- 4 single indirect pointers (`iblocks[4]`)
- 1 double indirect pointer (`i2block`)
- 1 triple indirect pointer (`i3block`)
- File metadata (size, nlink, uid, gid, times, etc.)

## Algorithm

1. Read entire fragmented disk image into memory
2. Parse superblock to determine region boundaries
3. Iterate through inode region and identify in-use files (`nlink > 0`)
4. For each file:
   - Read all data blocks (handling direct/indirect/double/triple indirect)
   - Write blocks contiguously to new data region
   - Update inode pointers to reflect new block locations
5. Create sorted free block list
6. Write defragmented disk image with updated structures

## Test Cases

Three test disk images provided:
- [`images_frag/disk_frag_1`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/images_frag) → [`images_defrag/disk_defrag_1`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/images_defrag)
- [`images_frag/disk_frag_2`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/images_frag) → [`images_defrag/disk_defrag_2`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/images_defrag)
- [`images_frag/disk_frag_3`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/images_frag) → [`images_defrag/disk_defrag_3`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/images_defrag)

## Expected Behavior

### Correct Defragmentation
- All file blocks laid out contiguously and sequentially
- Indirect block hierarchy maintained (indirect → data blocks)
- Boot block and swap space copied unchanged
- Free blocks set to zeros except first 4 bytes (next pointer)
- Free block list sorted in ascending order
- Unused inode/indirect pointers set to `-1`

### Performance Improvement
Defragmentation reduces seek time by ensuring sequential block reads, especially beneficial for:
- Large files with many scattered blocks
- Frequently accessed files
- HDDs with mechanical seek latency

## Notes/Assumptions
- Input disk images are valid and uncorrupted
- Unused inodes have `nlink == 0`
- In-use inodes have `nlink > 0`
- Inodes may span block boundaries
- Indirect blocks use `-1` for unused pointers
- Free blocks contain only zeros after the next-pointer field
- Compilation with `-O0` flag ensures proper memory alignment
- `sizeof(struct superblock)` must be 24 bytes
- `sizeof(struct inode)` must be 100 bytes
- Malloc returns 4-byte aligned addresses

## Acknowledgments

This project was completed as part of the graduate-level Operating Systems course (Fall 2025), **CSC 501: Operating Systems Principles**, at North Carolina State University, instructed by **Prof. Man-Ki Yoon**.

## Additional Resources
- See [ASSIGNMENT.md](ASSIGNMENT.md) for complete project specification
- Diff scripts in [`diff_scripts/`](https://github.com/devangsaraogi/xinu-filesystem-defragmenter/tree/main/diff_scripts) directory for output validation
