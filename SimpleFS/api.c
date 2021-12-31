#include "constants.h"
#include "directoryTable.h"
#include "disk_emu.h"
#include "fileDescriptorTable.h"
#include "freeBitMap.h"
#include "inode.h"
#include "sfs_api.h"
#include "superblock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Location of important structures in memory
int SUPERBLOCK_BLOCKNUM = 0;
int FREEBITMAP_BLOCKNUM = 1;
int DIRECTORY_BLOCKNUM = 2;
int INODECACHE_BLOCKNUM_START = 3;
int INODECACHE_BLOCKNUM_END = 6;

superblock s;
inodeCache icache;
directoryTable dTable;
freeBitMap *bitMap;
fileDescriptorTable fdt;
int directoryIndex = 0;


/*
* Formats the virtual disk implemented by the disk emulator and creates an 
* instance of the simple file system on top of it
*/
void mksfs(int fresh)
{
    // Create file system from scratch
    if(fresh)
    {
        init_fresh_disk("FileSystem", BLOCKSIZE, NUM_BLOCKS);

        /****** INITIALIZE FREE BIT MAP ******/
        // 1. Create free bit map
        bitMap = create_freeBitMap();
        // 2. Write free bit map onto disk
        char *fbmBuffer = (char *)malloc(sizeof(*bitMap));
        memcpy(fbmBuffer, bitMap, sizeof(*bitMap));
        write_blocks(FREEBITMAP_BLOCKNUM, 1, fbmBuffer);
        free(fbmBuffer);
        // 3. Set used block in free bit map
        set_bit(bitMap, FREEBITMAP_BLOCKNUM);

        /****** INITIALIZE SUPER BLOCK ******/
        // 1. Create super block
        superblock_create(&s);
        //s = superblock_create();
        // 2. Write super block to first block
        // char *sbBuffer = (char*) malloc(sizeof(s));
        // memcpy(sbBuffer, s, sizeof(s));
        // write_blocks(SUPERBLOCK_BLOCKNUM, 1, sbBuffer);
        // free(sbBuffer);
        write_blocks(SUPERBLOCK_BLOCKNUM, 1, &s);
        // 3. Set used block in free bit map
        set_bit(bitMap, SUPERBLOCK_BLOCKNUM);

        /****** INITIALIZE DIRECTORY ******/
        createDirectoryTable(&dTable);
        write_blocks(DIRECTORY_BLOCKNUM, 1, &dTable);
        set_bit(bitMap, DIRECTORY_BLOCKNUM);

        /****** INITIALIZE INODE CACHE ******/
        // 1. Create inode cache
        //icache = create_inodeCache();
        create_inodeCache(&icache);
        // 2. Write inode cache blocks
        // char* iBuffer = (char*)malloc(sizeof(icache));
        // memcpy(iBuffer, icache, sizeof(icache));
        // write_blocks(INODECACHE_BLOCKNUM_START, MAX_NUM_ICACHE_BLOCKS, iBuffer);
        // free(iBuffer);
        write_blocks(INODECACHE_BLOCKNUM_START, MAX_NUM_ICACHE_BLOCKS, &icache);
        // 3. Set used blocks in free bit map
        for(int i = 0; i <= 4; i++)
        {
            set_bit(bitMap, INODECACHE_BLOCKNUM_START + i);
        }
    }
    else
    {
        init_disk("FileSystem", BLOCKSIZE, NUM_BLOCKS);
    }
}

/*
* Copies the name of the next file in the directory into buffer
* and returns non zero if there is a new file
*/
int sfs_getnextfilename(char* buffer)
{
    // directoryIndex is global variable, indicating which entry of directory is next

    // if directoryIndex exceeds max files allowed, we know we must return to the start
    if(directoryIndex >= MAX_FILES)
    {
        directoryIndex = 0;
        return 0;
    }
    // check if directory entry is being used
    else if(directoryTable_entryStatus(&dTable, directoryIndex))
    {
        // copy name of file at directoryIndex into buffer
        directoryTable_getName(&dTable, directoryIndex, buffer);
        directoryIndex++;
        return 1;
    }
    else
    {
        // directory organizes such that no unused entries between used entries
        // so if entry is unused, we know we must return to the start
        directoryIndex = 0;
        return 0;
    }
}

/*
* Returns the size of a given file
*/
int sfs_getfilesize(const char* filename)
{
    // if file name exceeds max length, then it cannot be in directory
    if(strlen(filename) > MAX_FILE_NAME_LEN + MAX_FILE_EXT_LEN + 1)
    {
        return -1;
    }

    int inode;
    // Check to see if file exists in directory
    int fileFound = directoryTable_findFile(&dTable, filename);
    if (fileFound >= 0)
    {
        // inodes store the size of a file
        inode = directoryTable_getInode(&dTable, fileFound);
        int size = inode_size(&icache, inode);
        return size;
    }
    return -1; //file not found
}

/*
* Opens a file and returns the index that corresponds to the newly 
* opened file in the file descriptor table. If the file does not exist, it creates
* a new file and sets its size to 0. If the file exists, the file is opened in append mode
*/
int sfs_fopen(char* filename)
{
    // if filename length exceeds maximum, cannot create file
    if(strlen(filename) > MAX_FILE_NAME_LEN + MAX_FILE_EXT_LEN + 1)
    {
        return -1;
    }

    int inode;
    // Check to see if file already exists in directory
    int fileFound = directoryTable_findFile(&dTable, filename);

    if (fileFound >= 0) // open already created file
    {
        inode = directoryTable_getInode(&dTable, fileFound);
        //inode_incrementNumLinks(&icache, inode);
        int size = inode_size(&icache, inode);
        // add opened file to the file descriptor table
        fdt_addEntry(&fdt, inode, size);
    }
    else // create a new file and open it
    {
        inode = inode_newFile(&icache); // create inode for the file
        directoryTable_addEntry(&dTable, filename, inode); // add file to the directory
        write_blocks(DIRECTORY_BLOCKNUM, 1, &dTable);

        fdt_addEntry(&fdt, inode, 0); // add opened file to the file descriptor table
    }
    return inode;
}

/*
* Closes a file
*/
int sfs_fclose(int fd)
{
    // Remove file from file descriptor table
    // if file already closed return -1
    if(fdt_closeEntry(&fdt, fd) == -1)
    {
        return -1;
    }
    //inode_decrementNumLinks(&icache, fd);
    return 0;
}

// write to numBytes of buffer to block starting at offset
void write_incompleteblock(int block, void* buffer, int offset, int numBytes)
{
    char* blockData = (char*) malloc(sizeof(char) * BLOCKSIZE);
    read_blocks(block, 1, blockData);

    memcpy(blockData + offset, buffer, numBytes);
    write_blocks(block, 1, blockData);
    free(blockData);
}

/*
* writes the given number of bytes of buffered data in buffer into the open file, 
* starting from the current file pointer
*/
int sfs_fwrite(int fd, const char* buffer, int size)
{
    // check to see if file is open
    if(fdt_entryStatus(&fdt, fd) == 0)
    {
        return 0;
    }

    int bytesWritten = 0;
    int* blocks = (int*)malloc(MAX_NUM_FILE_BLOCKS*4);
    // stores file block numbers used by file in blocks and returns number of blocks used
    int fileBlocks_used = inode_getBlockNums(&icache, bitMap, fd, blocks);
    int blockToWriteTo = get_rwPtr(&fdt, fd)/BLOCKSIZE; //Determine index of first block to write to in blocks
    
    //Location of last byte written
    int bytesToWrite = size + get_rwPtr(&fdt, fd);

    // Can't write more than maximum allowed
    if(bytesToWrite > MAX_FILE_SIZE)
    {
        return 0;
    }
    
    // Need to allocate more blocks to file
    if (bytesToWrite > fileBlocks_used * BLOCKSIZE)
    {
        int num_newBlocks = (bytesToWrite - (fileBlocks_used * BLOCKSIZE))/BLOCKSIZE;
        //integer division takes floor of division, determine whether it should be one higher
        double temp = ((double)bytesToWrite - ((double)fileBlocks_used * (double)BLOCKSIZE))/(double)BLOCKSIZE;
        if (temp > num_newBlocks) {num_newBlocks++;}

        int i;
        for(i = 0; i < num_newBlocks; i++)
        {
            // Add a direct block to file
            if(fileBlocks_used < 12)
            {
                // Creates new direct block and returns block number
                int blocknum = inode_getDirPtrIndex(&icache, bitMap, fd, fileBlocks_used);
                // Add block number to list of used blocks
                blocks[fileBlocks_used] = blocknum;
            }
            // Add first indirect block to file
            else if (fileBlocks_used == 12)
            {
                // Creates new indirect index block and returns block number
                int indPtrIndex = inode_getIndirectPtr(&icache, bitMap, fd);
                
                // Find free block for data, add block number as index to indirect ptr index block
                int blocknum = get_freeBit(bitMap);
                if (blocknum == -1)
                {
                    return 0;
                }
                set_bit(bitMap, blocknum);
                blocks[fileBlocks_used] = blocknum;
                int* num = &blocknum;
                write_incompleteblock(indPtrIndex, num, 0, 4);
            }
            // Add indirect block
            else
            {
                // gets indirect index block number
                int indPtrIndex = inode_getIndirectPtr(&icache, bitMap, fd);
                // Find free block for data, add block number as index to indirect ptr index block
                int blocknum = get_freeBit(bitMap);
                if (blocknum == -1)
                {
                    return 0;
                }

                set_bit(bitMap, blocknum);
                blocks[fileBlocks_used] = blocknum;
                int* num = &blocknum;
                write_incompleteblock(indPtrIndex, num, (fileBlocks_used - 12)*4, 4);
            }
            
            fileBlocks_used++;

        }
    }
    
    char * copy = (char *) malloc(size*(sizeof(char*)));
    memcpy(copy, buffer, size);
    char *bufferptr = copy;
    
    int bytesToFirstBlock = 0;
    // Determine how much data can fit into block rwptr is in
    if (size < BLOCKSIZE - (get_rwPtr(&fdt, fd) % BLOCKSIZE))
    {
        bytesToFirstBlock = size;
    }
    else{
        bytesToFirstBlock = BLOCKSIZE - (get_rwPtr(&fdt, fd) % BLOCKSIZE);
    }
    
    // write data into remaining space in block that rwptr is pointing to
    write_incompleteblock(blocks[blockToWriteTo], bufferptr, (get_rwPtr(&fdt, fd) % BLOCKSIZE), bytesToFirstBlock);
    bufferptr += bytesToFirstBlock;
    bytesWritten += bytesToFirstBlock;

    // Determine the amount of data that will be written to last block
    int lastBytes =(size - bytesToFirstBlock) % BLOCKSIZE;
    
    int j;
    blockToWriteTo++; // increment index of block to write to in blocks
    for(j = 0; j < (size - bytesToFirstBlock) / BLOCKSIZE; j++)
    {
        write_blocks(blocks[blockToWriteTo + j], 1, bufferptr);
        bufferptr += BLOCKSIZE;
        bytesWritten += BLOCKSIZE;

    }

    if (lastBytes > 0)
    {
        write_incompleteblock(blocks[j + blockToWriteTo], bufferptr, 0, lastBytes);
        bytesWritten += lastBytes;
    }

    // set rwPtr to end of file
    int old_rwPtr = get_rwPtr(&fdt, fd);
    set_rwPtr(&fdt, fd, old_rwPtr + bytesWritten);
    // increase file size by bytes written
    inode_incrementSize(&icache, fd, bytesWritten);

    // write updated free bit map to disk
    char *fbmBuffer = (char *)malloc(sizeof(*bitMap));
    memcpy(fbmBuffer, bitMap, sizeof(*bitMap));
    write_blocks(FREEBITMAP_BLOCKNUM, 1, fbmBuffer);
    free(fbmBuffer);
    
    free(blocks);
    
    return bytesWritten;
}

// read numBytes from block starting at offset into buffer
void read_incompleteblock(int block, char* buffer, int offset, int numBytes)
{
    char* blockData = (char*) malloc(sizeof(char) * BLOCKSIZE);
    read_blocks(block, 1, blockData);

    memcpy(buffer, blockData + offset, numBytes);
    free(blockData); 
}

int sfs_fread(int fd, char* buffer, int size)
{
    // Check to see if file is open
    if(fdt_entryStatus(&fdt, fd) == 0)
    {
        return 0;
    }

    char *bufferptr = buffer;
    int fileSize = inode_size(&icache, fd);
    int file_rwPtr = get_rwPtr(&fdt, fd);

    int blockToStartRead = file_rwPtr/BLOCKSIZE;
    int endOfRead = file_rwPtr + size;
    int readBytes = 0;

    // Can only read data within file
    if (fileSize < endOfRead) {size = fileSize - file_rwPtr;}

    int* blocks = (int*) malloc(MAX_NUM_FILE_BLOCKS *4);
    int fileBlocks_used = inode_getBlockNums(&icache, bitMap, fd, blocks);

    
    int bytesToFirstBlock = 0;
    // Determine number of bytes to read from block rwPtr is in
    if (size < BLOCKSIZE - (get_rwPtr(&fdt, fd) % BLOCKSIZE))
    {
        bytesToFirstBlock = size;
    }
    else{
        bytesToFirstBlock = BLOCKSIZE - (get_rwPtr(&fdt, fd) % BLOCKSIZE);
    }

    // read from block rwPtr is in
    read_incompleteblock(blocks[blockToStartRead], bufferptr, (get_rwPtr(&fdt, fd) % BLOCKSIZE), bytesToFirstBlock);
    bufferptr += bytesToFirstBlock;
    readBytes += bytesToFirstBlock;

    // Determine number of bytes to read from last block
    int lastBytes =(size - bytesToFirstBlock) % BLOCKSIZE;

    int j;
    blockToStartRead++;
    for(j = 0; j < (size - bytesToFirstBlock) / BLOCKSIZE; j++)
    {
        read_blocks(blocks[blockToStartRead + j], 1, bufferptr);
        bufferptr += BLOCKSIZE;
        readBytes += BLOCKSIZE;

    }

    if (lastBytes > 0)
    {
        read_incompleteblock(blocks[j + blockToStartRead], bufferptr, 0, lastBytes);
        readBytes += lastBytes;
    }

    // Set rwPtr to end of read section
    int old_rwPtr = get_rwPtr(&fdt, fd);
    set_rwPtr(&fdt, fd, old_rwPtr + readBytes);
    free(blocks);

    return readBytes;
}
/*
* moves the read/write pointer to the given location
*/
int sfs_fseek(int fd, int location)
{
    if (location >= MAX_FILE_SIZE || location < 0)
    {
        return -1;
    }
    set_rwPtr(&fdt, fd, location);

    return 0;
}

int sfs_remove(char* filename)
{
    int inode;
    // Check to see if file exists in file desriptor table
    int fileFound = directoryTable_findFile(&dTable, filename);

    if (fileFound >= 0)
    {
        inode = directoryTable_getInode(&dTable, fileFound);
        int* blocks = (int*) malloc(MAX_NUM_FILE_BLOCKS *4);
        int fileBlocks_used = inode_getBlockNums(&icache, bitMap, inode, blocks);
        
        // Free all of the blocks used by the file
        for(int i = 0; i < fileBlocks_used; i++)
        {
            unset_bit(bitMap, blocks[i]);
        }
        
        // Remove inode from inode cache
        inode_removeFile(&icache, inode);
        // Remove entry from file descriptor table
        fdt_removeEntry(&fdt, inode);
        // Remove entry from directory
        directoryTable_removeEntry(&dTable, fileFound);

        char *fbmBuffer = (char *)malloc(sizeof(*bitMap));
        memcpy(fbmBuffer, bitMap, sizeof(*bitMap));
        write_blocks(FREEBITMAP_BLOCKNUM, 1, fbmBuffer);
        free(fbmBuffer);

        write_blocks(DIRECTORY_BLOCKNUM, 1, &dTable);
    
        free(blocks);  
     }
    else
    {
        return -1;
    }
    
    return 1;
}