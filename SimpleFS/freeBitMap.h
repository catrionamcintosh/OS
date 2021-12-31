#ifndef FREEBITMAP_H
#define FREEBITMAP_H
#include "constants.h"
#include <stdlib.h>
#include <math.h>

typedef struct
{
    int numBits;
    char* map;
} freeBitMap;

// Set bit to 1 so that it shows that the block is used
void set_bit(freeBitMap *bMap, int index)
{
    int byte_index = index >> 3;
    int offset = ((unsigned) index << 29) >> 29;
    bMap->map[byte_index] |= 1 << (7 - offset);
}

// void set_bit(freeBitMap *bMap, int index)
// {
//     bMap->map[index] = 1;
// }

// Set bit to 0 to show that it is free
void unset_bit(freeBitMap *bMap, int index)
{
    int byte_index = index >> 3;
    int offset = ((unsigned) index << 29) >> 29;
    bMap->map[byte_index] &= ~(1 << (7 - offset));
}

// void unset_bit(freeBitMap *bMap, int index)
// {
//     bMap->map[index] = 0;
// }

// return if bit is used
int get_bit(freeBitMap *bMap, int index)
{
    int byte_index = index >> 3;
    int offset = ((unsigned) index << 29) >> 29;
    if (bMap->map[byte_index] & (1 << (7-offset)))
    {
        return 1;
    }

    return 0;

}
// int get_bit(freeBitMap *bMap, int index)
// {
//     return bMap->map[index];
// }

// return index of a free bit, representing a free block
int get_freeBit(freeBitMap *bMap)
{
    for(int i = 0; i < bMap->numBits; i++)
    {
        if(get_bit(bMap, i) == 0)
        {
            return i;
        }
    }

    return -1;
}
// create a free bit map for this file system specifically
freeBitMap* create_freeBitMap()
{
    freeBitMap* bitMap = (freeBitMap*) malloc(sizeof(bitMap));
    bitMap->numBits = NUM_BLOCKS;
    int bytes = (int) ceil(NUM_BLOCKS/8.0);
    bitMap->map = (char*)malloc(bytes);
    return bitMap;
}

// freeBitMap* create_freeBitMap()
// {
//     freeBitMap* bitMap = (freeBitMap*) malloc(sizeof(bitMap));
//     bitMap->numBits = NUM_BLOCKS;
//     bitMap->map = (char*)malloc(NUM_BLOCKS);
//     return bitMap;
// }

//create a free bit map of with size number of bits
freeBitMap* create_bitMap(int size)
{
    freeBitMap* bitMap = (freeBitMap*) malloc(sizeof(bitMap));
    bitMap->numBits = size;
    int bytes = (int) ceil(NUM_BLOCKS/8.0);
    bitMap->map = (char*)malloc(bytes);

    return bitMap;
}
// freeBitMap* create_bitMap(int size)
// {
//     freeBitMap* bitMap = (freeBitMap*) malloc(sizeof(bitMap));
//     bitMap->numBits = size;
//     bitMap->map = (char*)malloc(size);
//     return bitMap;
// }

#endif