#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOOT_SIZE 512
#define SUPER_SIZE 512
#define N_DBLOCKS 10
#define N_IBLOCKS 4

struct superblock
{
    int blocksize;    /* size of blocks in bytes */
    int inode_offset; /* offset of inode region in blocks */
    int data_offset;  /* data region offset in blocks */
    int swap_offset;  /* swap region offset in blocks */
    int free_inode;   /* head of free inode list */
    int free_block;   /* head of free block list */
};

struct inode
{
    int next_inode;         /* list for free inodes */
    int protect;            /*  protection field */
    int nlink;              /* Number of links to this file */
    int size;               /* Number of bytes in file */
    int uid;                /* Owner's user ID */
    int gid;                /* Owner's group ID */
    int ctime;              /* Time field */
    int mtime;              /* Time field */
    int atime;              /* Time field */
    int dblocks[N_DBLOCKS]; /* Pointers to data blocks */
    int iblocks[N_IBLOCKS]; /* Pointers to indirect blocks */
    int i2block;            /* Pointer to doubly indirect block */
    int i3block;            /* Pointer to triply indirect block */
};

// Global variables
unsigned char *input_disk;
unsigned char *output_disk;
struct superblock super;
long total_size;
long inode_start;
long data_start;
long swap_start;

// Helper function to calculate blocks needed
int get_blocks_needed(int file_size)
{
    if (file_size == 0)
        return 0;
    int blocks = file_size / super.blocksize;
    if (file_size % super.blocksize != 0)
    {
        blocks++;
    }
    return blocks;
}

// Helper function to read direct blocks
int read_direct_blocks(struct inode *in_inode, unsigned char **file_data, int blocks_needed)
{
    int blocks_read = 0;
    int j;
    for (j = 0; j < N_DBLOCKS; j++)
    {
        if (blocks_read >= blocks_needed)
            break;
        if (in_inode->dblocks[j] == -1)
            break;

        long offset = data_start + in_inode->dblocks[j] * super.blocksize;
        int k;
        for (k = 0; k < super.blocksize; k++)
        {
            file_data[blocks_read][k] = input_disk[offset + k];
        }
        blocks_read++;
    }
    return blocks_read;
}

// Helper function to read single indirect blocks
int read_single_indirect(struct inode *in_inode, unsigned char **file_data, int blocks_needed, int blocks_read)
{
    int ptrs_per_block = super.blocksize / 4;
    int j;
    for (j = 0; j < N_IBLOCKS; j++)
    {
        if (blocks_read >= blocks_needed)
            break;
        if (in_inode->iblocks[j] == -1)
            break;

        long indirect_offset = data_start + in_inode->iblocks[j] * super.blocksize;
        int *ptrs = (int *)(input_disk + indirect_offset);

        int p;
        for (p = 0; p < ptrs_per_block; p++)
        {
            if (blocks_read >= blocks_needed)
                break;
            if (ptrs[p] == -1)
                break;

            long offset = data_start + ptrs[p] * super.blocksize;
            int k;
            for (k = 0; k < super.blocksize; k++)
            {
                file_data[blocks_read][k] = input_disk[offset + k];
            }
            blocks_read++;
        }
    }
    return blocks_read;
}

// Helper function to read double indirect blocks
int read_double_indirect(struct inode *in_inode, unsigned char **file_data, int blocks_needed, int blocks_read)
{
    if (in_inode->i2block == -1)
        return blocks_read;

    int ptrs_per_block = super.blocksize / 4;
    long i2_offset = data_start + in_inode->i2block * super.blocksize;
    int *level1 = (int *)(input_disk + i2_offset);

    int j;
    for (j = 0; j < ptrs_per_block; j++)
    {
        if (blocks_read >= blocks_needed)
            break;
        if (level1[j] == -1)
            break;

        long indirect_offset = data_start + level1[j] * super.blocksize;
        int *level2 = (int *)(input_disk + indirect_offset);

        int p;
        for (p = 0; p < ptrs_per_block; p++)
        {
            if (blocks_read >= blocks_needed)
                break;
            if (level2[p] == -1)
                break;

            long offset = data_start + level2[p] * super.blocksize;
            int k;
            for (k = 0; k < super.blocksize; k++)
            {
                file_data[blocks_read][k] = input_disk[offset + k];
            }
            blocks_read++;
        }
    }
    return blocks_read;
}

// Helper function to read triple indirect blocks
int read_triple_indirect(struct inode *in_inode, unsigned char **file_data, int blocks_needed, int blocks_read)
{
    if (in_inode->i3block == -1)
        return blocks_read;

    int ptrs_per_block = super.blocksize / 4;
    long i3_offset = data_start + in_inode->i3block * super.blocksize;
    int *level1 = (int *)(input_disk + i3_offset);

    int j;
    for (j = 0; j < ptrs_per_block; j++)
    {
        if (blocks_read >= blocks_needed)
            break;
        if (level1[j] == -1)
            break;

        long i2_offset = data_start + level1[j] * super.blocksize;
        int *level2 = (int *)(input_disk + i2_offset);

        int p;
        for (p = 0; p < ptrs_per_block; p++)
        {
            if (blocks_read >= blocks_needed)
                break;
            if (level2[p] == -1)
                break;

            long indirect_offset = data_start + level2[p] * super.blocksize;
            int *level3 = (int *)(input_disk + indirect_offset);

            int q;
            for (q = 0; q < ptrs_per_block; q++)
            {
                if (blocks_read >= blocks_needed)
                    break;
                if (level3[q] == -1)
                    break;

                long offset = data_start + level3[q] * super.blocksize;
                int k;
                for (k = 0; k < super.blocksize; k++)
                {
                    file_data[blocks_read][k] = input_disk[offset + k];
                }
                blocks_read++;
            }
        }
    }
    return blocks_read;
}

// Helper function to write direct blocks to output
int write_direct_blocks(unsigned char **file_data, struct inode *out_inode, int blocks_needed, int next_block)
{
    int blocks_written = 0;
    int j;
    for (j = 0; j < N_DBLOCKS; j++)
    {
        if (blocks_written >= blocks_needed)
            break;

        long offset = data_start + next_block * super.blocksize;
        int k;
        for (k = 0; k < super.blocksize; k++)
        {
            output_disk[offset + k] = file_data[blocks_written][k];
        }

        out_inode->dblocks[j] = next_block;
        next_block++;
        blocks_written++;
    }

    // Set unused pointers to -1
    for (j = blocks_written; j < N_DBLOCKS; j++)
    {
        out_inode->dblocks[j] = -1;
    }

    return blocks_written;
}

// Helper function to write single indirect blocks
int write_single_indirect(unsigned char **file_data, struct inode *out_inode, int blocks_needed, int blocks_written, int *next_block_ptr)
{
    int next_block = *next_block_ptr;
    int ptrs_per_block = super.blocksize / 4;
    int indirect_used = 0;

    int j;
    for (j = 0; j < N_IBLOCKS; j++)
    {
        if (blocks_written >= blocks_needed)
            break;

        // Allocate indirect block
        int iblock_num = next_block;
        next_block++;

        long iblock_offset = data_start + iblock_num * super.blocksize;
        int *iblock_ptrs = (int *)(output_disk + iblock_offset);

        int k;
        for (k = 0; k < super.blocksize; k++)
        {
            output_disk[iblock_offset + k] = 0;
        }

        // Write data blocks
        int ptr_count = 0;
        for (k = 0; k < ptrs_per_block; k++)
        {
            if (blocks_written >= blocks_needed)
                break;

            int data_block_num = next_block;
            next_block++;

            iblock_ptrs[k] = data_block_num;

            long offset = data_start + data_block_num * super.blocksize;
            int m;
            for (m = 0; m < super.blocksize; m++)
            {
                output_disk[offset + m] = file_data[blocks_written][m];
            }

            blocks_written++;
            ptr_count++;
        }

        // Set remaining pointers to -1
        for (k = ptr_count; k < ptrs_per_block; k++)
        {
            iblock_ptrs[k] = -1;
        }

        out_inode->iblocks[j] = iblock_num;
        indirect_used++;
    }

    // Set unused indirect blocks to -1
    for (j = indirect_used; j < N_IBLOCKS; j++)
    {
        out_inode->iblocks[j] = -1;
    }

    *next_block_ptr = next_block;
    return blocks_written;
}

// Helper function to write double indirect blocks
int write_double_indirect(unsigned char **file_data, struct inode *out_inode, int blocks_needed, int blocks_written, int *next_block_ptr)
{
    if (blocks_written >= blocks_needed)
    {
        out_inode->i2block = -1;
        return blocks_written;
    }

    int next_block = *next_block_ptr;
    int ptrs_per_block = super.blocksize / 4;

    int i2block_num = next_block;
    next_block++;

    long i2block_offset = data_start + i2block_num * super.blocksize;
    int *i2block_ptrs = (int *)(output_disk + i2block_offset);

    //
    for (int k = 0; k < super.blocksize; k++)
    {
        output_disk[i2block_offset + k] = 0;
    }

    int i2_count = 0;
    while (blocks_written < blocks_needed && i2_count < ptrs_per_block)
    {
        // Allocate indirect block
        int iblock_num = next_block;
        next_block++;

        i2block_ptrs[i2_count] = iblock_num;
        i2_count++;

        long iblock_offset = data_start + iblock_num * super.blocksize;
        int *iblock_ptrs = (int *)(output_disk + iblock_offset);

        for (int k = 0; k < super.blocksize; k++)
        {
            output_disk[iblock_offset + k] = 0;
        }

        // Write data blocks
        int ptr_count = 0;
        for (int k = 0; k < ptrs_per_block; k++)
        {
            if (blocks_written >= blocks_needed)
                break;

            int data_block_num = next_block;
            next_block++;

            iblock_ptrs[k] = data_block_num;

            long offset = data_start + data_block_num * super.blocksize;
            int m;
            for (m = 0; m < super.blocksize; m++)
            {
                output_disk[offset + m] = file_data[blocks_written][m];
            }

            blocks_written++;
            ptr_count++;
        }

        // Set remaining pointers to -1
        for (int k = ptr_count; k < ptrs_per_block; k++)
        {
            iblock_ptrs[k] = -1;
        }
    }

    // Set remaining i2 pointers to -1
    for (int k = i2_count; k < ptrs_per_block; k++)
    {
        i2block_ptrs[k] = -1;
    }

    out_inode->i2block = i2block_num;
    *next_block_ptr = next_block;
    return blocks_written;
}

// Helper function to write triple indirect blocks
int write_triple_indirect(unsigned char **file_data, struct inode *out_inode, int blocks_needed, int blocks_written, int *next_block_ptr)
{
    if (blocks_written >= blocks_needed)
    {
        out_inode->i3block = -1;
        return blocks_written;
    }

    int next_block = *next_block_ptr;
    int ptrs_per_block = super.blocksize / 4;

    int i3block_num = next_block;
    next_block++;

    long i3block_offset = data_start + i3block_num * super.blocksize;
    int *i3block_ptrs = (int *)(output_disk + i3block_offset);

    for (int k = 0; k < super.blocksize; k++)
    {
        output_disk[i3block_offset + k] = 0;
    }

    int i3_count = 0;
    while (blocks_written < blocks_needed && i3_count < ptrs_per_block)
    {
        // Allocate double indirect block
        int i2block_num = next_block;
        next_block++;

        i3block_ptrs[i3_count] = i2block_num;
        i3_count++;

        long i2block_offset = data_start + i2block_num * super.blocksize;
        int *i2block_ptrs = (int *)(output_disk + i2block_offset);

        //
        for (int k = 0; k < super.blocksize; k++)
        {
            output_disk[i2block_offset + k] = 0;
        }

        int i2_count = 0;
        while (blocks_written < blocks_needed && i2_count < ptrs_per_block)
        {
            // Allocate indirect block
            int iblock_num = next_block;
            next_block++;

            i2block_ptrs[i2_count] = iblock_num;
            i2_count++;

            long iblock_offset = data_start + iblock_num * super.blocksize;
            int *iblock_ptrs = (int *)(output_disk + iblock_offset);

            //
            for (int k = 0; k < super.blocksize; k++)
            {
                output_disk[iblock_offset + k] = 0;
            }

            // Write data blocks
            int ptr_count = 0;
            for (int k = 0; k < ptrs_per_block; k++)
            {
                if (blocks_written >= blocks_needed)
                    break;

                int data_block_num = next_block;
                next_block++;

                iblock_ptrs[k] = data_block_num;

                long offset = data_start + data_block_num * super.blocksize;
                int m;
                for (m = 0; m < super.blocksize; m++)
                {
                    output_disk[offset + m] = file_data[blocks_written][m];
                }

                blocks_written++;
                ptr_count++;
            }

            // Set remaining pointers to -1
            for (int k = ptr_count; k < ptrs_per_block; k++)
            {
                iblock_ptrs[k] = -1;
            }
        }

        // Set remaining i2 pointers to -1
        for (int k = i2_count; k < ptrs_per_block; k++)
        {
            i2block_ptrs[k] = -1;
        }
    }

    // Set remaining i3 pointers to -1
    for (int k = i3_count; k < ptrs_per_block; k++)
    {
        i3block_ptrs[k] = -1;
    }

    out_inode->i3block = i3block_num;
    *next_block_ptr = next_block;
    return blocks_written;
}

// Function to process one file
void process_file(struct inode *in_inode, struct inode *out_inode, int *next_block_ptr)
{
    int file_size = in_inode->size;
    if (file_size == 0)
        return;

    int blocks_needed = get_blocks_needed(file_size);

    // Allocate array for file data
    unsigned char **file_data = (unsigned char **)malloc(blocks_needed * sizeof(unsigned char *));
    int b;
    for (b = 0; b < blocks_needed; b++)
    {
        file_data[b] = (unsigned char *)malloc(super.blocksize);
        int k;
        for (k = 0; k < super.blocksize; k++)
        {
            file_data[b][k] = 0;
        }
    }

    // Read all blocks from input
    int blocks_read = 0;
    blocks_read = read_direct_blocks(in_inode, file_data, blocks_needed);
    blocks_read = read_single_indirect(in_inode, file_data, blocks_needed, blocks_read);
    blocks_read = read_double_indirect(in_inode, file_data, blocks_needed, blocks_read);
    blocks_read = read_triple_indirect(in_inode, file_data, blocks_needed, blocks_read);

    // Write blocks contiguously to output
    int blocks_written = 0;
    int next_block = *next_block_ptr;

    blocks_written = write_direct_blocks(file_data, out_inode, blocks_needed, next_block);
    next_block += blocks_written;

    blocks_written = write_single_indirect(file_data, out_inode, blocks_needed, blocks_written, &next_block);
    blocks_written = write_double_indirect(file_data, out_inode, blocks_needed, blocks_written, &next_block);
    blocks_written = write_triple_indirect(file_data, out_inode, blocks_needed, blocks_written, &next_block);

    // Free memory
    for (b = 0; b < blocks_needed; b++)
    {
        free(file_data[b]);
    }
    free(file_data);

    *next_block_ptr = next_block;
}

// Function to copy boot, super, and swap regions
void copy_static_regions()
{
    int i;

    // Copy boot block
    for (i = 0; i < BOOT_SIZE; i++)
    {
        output_disk[i] = input_disk[i];
    }

    // Copy superblock
    for (i = BOOT_SIZE; i < BOOT_SIZE + SUPER_SIZE; i++)
    {
        output_disk[i] = input_disk[i];
    }

    // Copy inode region
    long inode_end = data_start;
    long inode_size = inode_end - inode_start;
    for (i = 0; i < inode_size; i++)
    {
        output_disk[inode_start + i] = input_disk[inode_start + i];
    }

    // Copy swap region
    long swap_size = total_size - swap_start;
    for (i = 0; i < swap_size; i++)
    {
        output_disk[swap_start + i] = input_disk[swap_start + i];
    }
}

// Function to create free block list
void create_free_list(int next_block)
{
    long total_blocks = (swap_start - data_start) / super.blocksize;
    int i;
    for (i = next_block; i < total_blocks; i++)
    {
        long offset = data_start + i * super.blocksize;

        int k;
        for (k = 0; k < super.blocksize; k++)
        {
            output_disk[offset + k] = 0;
        }

        // Set next pointer
        int *next_ptr = (int *)(output_disk + offset);
        if (i + 1 < total_blocks)
        {
            *next_ptr = i + 1;
        }
        else
        {
            *next_ptr = -1;
        }
    }
}

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc < 2)
    {
        printf("Error: Need filename\n");
        return 1;
    }

    // Check struct sizes
    if (sizeof(struct superblock) != 24)
    {
        printf("Superblock size wrong: %d\n", (int)sizeof(struct superblock));
        return 1;
    }
    if (sizeof(struct inode) != 100)
    {
        printf("Inode size wrong: %d\n", (int)sizeof(struct inode));
        return 1;
    }

    // Open input file
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL)
    {
        printf("Cannot open file\n");
        return 1;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    total_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate memory
    input_disk = (unsigned char *)malloc(total_size);
    output_disk = (unsigned char *)malloc(total_size);

    // Read file
    int bytes = fread(input_disk, 1, total_size, f);
    if (bytes != total_size)
    {
        printf("Read error\n");
        return 1;
    }
    fclose(f);

    // Clear output
    int i;
    for (i = 0; i < total_size; i++)
    {
        output_disk[i] = 0;
    }

    // Read superblock
    struct superblock *sb_ptr = (struct superblock *)&input_disk[BOOT_SIZE];
    super.blocksize = sb_ptr->blocksize;
    super.inode_offset = sb_ptr->inode_offset;
    super.data_offset = sb_ptr->data_offset;
    super.swap_offset = sb_ptr->swap_offset;
    super.free_inode = sb_ptr->free_inode;
    super.free_block = sb_ptr->free_block;

    // Calculate regions
    inode_start = BOOT_SIZE + SUPER_SIZE + super.inode_offset * super.blocksize;
    data_start = BOOT_SIZE + SUPER_SIZE + super.data_offset * super.blocksize;
    swap_start = BOOT_SIZE + SUPER_SIZE + ((long long)super.swap_offset) * super.blocksize;

    // Fix swap_start if needed
    if (swap_start > total_size || swap_start < 0)
    {
        swap_start = total_size;
    }

    // Copy static regions
    copy_static_regions();

    // Process each inode
    long inode_size = data_start - inode_start;
    int total_inodes = inode_size / 100;
    int next_block = 0;

    int inode_num;
    for (inode_num = 0; inode_num < total_inodes; inode_num++)
    {
        struct inode *in_inode = (struct inode *)(input_disk + inode_start + inode_num * 100);
        struct inode *out_inode = (struct inode *)(output_disk + inode_start + inode_num * 100);

        if (in_inode->nlink == 0)
            continue;

        process_file(in_inode, out_inode, &next_block);
    }

    // Update superblock
    struct superblock *out_sb = (struct superblock *)(output_disk + BOOT_SIZE);
    out_sb->free_block = next_block;

    // Create free block list
    create_free_list(next_block);

    // Write output file
    FILE *out = fopen("disk_defrag", "wb");
    if (out == NULL)
    {
        printf("Cannot create output file\n");
        return 1;
    }

    int written = fwrite(output_disk, 1, total_size, out);
    if (written != total_size)
    {
        printf("Write error\n");
        return 1;
    }

    fclose(out);
    free(input_disk);
    free(output_disk);

    return 0;
}