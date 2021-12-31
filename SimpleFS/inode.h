#include "constants.h"
#include "freeBitMap.h"

struct inode
{
    int size;
    //directPtrs is an array of block numbers for blocks that hold the data
    int directPtrs[NUM_DIR_PTRS];
    //indirectPtr holds a block number for a block that holds ptrs to other blocks
    int indirectPtr;
} typedef inode;

struct inodeCache
{
    inode cache[MAX_INODES];
    freeBitMap *freeInodes;
} typedef inodeCache;

// Creates inode cache to store inodes in file system
void create_inodeCache(inodeCache* icache)
{

    //root
    //create a free inode map, to find free inode when we need to create one
    freeBitMap *map = create_bitMap(MAX_INODES);
    icache->freeInodes = map;
    set_bit(icache->freeInodes, 0);//inode 0 is the root directory

    // each file is initially 0 size
    for(int i = 1; i < MAX_FILES; i++)
    {
        icache->cache[i].size = 0;
    }
}

// Return size of file
int inode_size(inodeCache* icache, int inode)
{
    return icache->cache[inode].size;
}

// Update file size
void inode_incrementSize(inodeCache* icache, int inode, int x)
{
    icache->cache[inode].size += x; 
}

// Creating a new file
// Setup inode that will store data blocks for that file
int inode_newFile(inodeCache* icache)
{
    // Find free inode
    int index = get_freeBit(icache->freeInodes);
    if (index > 0)
    {
        // set inode as used
        set_bit(icache->freeInodes, index);
        
        //set block nums = -1
        for(int i = 0; i < NUM_DIR_PTRS; i++)
        {
            icache->cache[index].directPtrs[i] = -1;
        }
        icache->cache[index].indirectPtr = -1;
        icache->cache[index].size = 0;
    }

    return index;
}

// If removing file, set the inode it uses to free
int inode_removeFile(inodeCache *icache, int inode)
{
    unset_bit(icache->freeInodes, inode);
    return 1;
}

/*
* Returns the block number of the ith direct pointer
*/
int inode_getDirPtrIndex(inodeCache *icache, freeBitMap *bitMap, int inode, int i)
{
    int index = icache->cache[inode].directPtrs[i];
    // If no block has been allocated to the ith direct pointer
    // then allocate a block to that direct ptr
    if (index == -1)
    {
        int blocknum = get_freeBit(bitMap);
        set_bit(bitMap, blocknum);
        icache->cache[inode].directPtrs[i] = blocknum;
        return blocknum;
    }

    return index;
}

// Returns block number of the indirect pointer
int inode_getIndirectPtr(inodeCache *icache, freeBitMap *bitMap, int inode)
{
    int index = icache->cache[inode].indirectPtr;
    // If no block has been allocated to the indirect pointer
    // then allocate a block to the indirect pointer
    if(index == -1)
    {
        //Error handle: no free bits
        int blocknum = get_freeBit(bitMap);
        set_bit(bitMap, blocknum);
        icache->cache[inode].indirectPtr = blocknum;
        return blocknum;
    }

    return index;
}

// Write all of the block numbers of the direct pointers and indirect pointer data blocks 
// used by the file represented by inode into blocks. Returns number of blocks used
int inode_getBlockNums(inodeCache *icache, freeBitMap *bitMap, int inode, int *blocks)
{
    int size = inode_size(icache, inode);
    int num_dirPtrs = 0;
    int num_indPtrs = 0;
    double temp= 0.0;

    // If uses indirect pointer    
    if (size > DIR_PTR_BYTES)
    {
        num_dirPtrs = 12;
        temp = ((double) size - (double) DIR_PTR_BYTES)/ (double) BLOCKSIZE;
        num_indPtrs = (size - DIR_PTR_BYTES)/BLOCKSIZE;
        if (temp > (double) num_indPtrs)
        {
            num_indPtrs++;
        }

        //Error handle file too large
        // Add tth direct pointer block number to blocks at index t
        for(int t = 0; t < 12; t++)
        {
            blocks[t] = inode_getDirPtrIndex(icache, bitMap, inode, t);
        }
        // Read the block numbers stored in the indirect pointer index block and add them to blocks
        char* buffer = (char*)malloc(BLOCKSIZE);
        read_blocks(inode_getIndirectPtr(icache, bitMap, inode), 1, buffer);
        memcpy((blocks + 12), buffer, 4*num_indPtrs);
        free(buffer);
    }
    else // no indirect pointer used
    {
        temp = (double) size/ (double) BLOCKSIZE;
        num_dirPtrs = size/BLOCKSIZE;
        if (temp > (double) num_dirPtrs)
        {
            num_dirPtrs++;
        }

        // add direct data block block numbers to blocks
        for(int i = 0; i < num_dirPtrs; i++)
        {
            blocks[i] = inode_getDirPtrIndex(icache, bitMap, inode, i);
        }

    }

    return num_dirPtrs + num_indPtrs;
    

}