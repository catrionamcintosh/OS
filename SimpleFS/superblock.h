#include "constants.h"

struct superblock
{
    int magic;
    int blockSize;
    int fileSystemSize;
    int inodeTableLength;
    int rootDir;
} typedef superblock;

void superblock_create(superblock *super)
{
    super->magic = MAGIC;
    super->blockSize = BLOCKSIZE;
    super->fileSystemSize = NUM_BLOCKS;
    super->inodeTableLength = MAX_INODES;
    super->rootDir = 0;
}