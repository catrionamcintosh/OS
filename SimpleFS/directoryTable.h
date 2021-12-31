#include "constants.h"
#include <string.h>
#include <stdlib.h>

typedef struct 
{
    //Directory entry maps file name to inode
    int inode;
    char fname[MAX_FILE_NAME_LEN+MAX_FILE_EXT_LEN + 1 + 1];
    int used; //keeps track of whether entry is being used
} dentry;

typedef struct
{
    int numEntries;
    dentry directory[MAX_FILES];
} directoryTable;

// returns whether directory entry is being used
int directoryTable_entryStatus(directoryTable *t, int index)
{
    return t->directory[index].used;
}

// creates directory, initializes fields
void createDirectoryTable(directoryTable *dTable)
{
    dTable->numEntries = 0;
    for(int i = 0; i < MAX_FILES; i++)
    {
        dTable->directory[i].used = 0;
    }
}

int directoryTable_addEntry(directoryTable *t, char *fileName, int inodeNum)
{
    int entry = t->numEntries;
    if (entry >= MAX_FILES)
    {
        return -1;
    }

    t->directory[entry].inode = inodeNum;
    strcpy(t->directory[entry].fname, fileName);
    t->directory[entry].used = 1;
    t->numEntries++;

    return 0;//returns index in directory table of entry
}

int directoryTable_removeEntry(directoryTable *t, int index)
{
    // shifts all used entries after entry at index to the left
    for(int i = index; i < t->numEntries; i++)
    {
        if(t->directory[i+1].used == 0)
        {
            t->directory[i].used =0;
            t->numEntries--;
            return 1;
        }
        else
        {
            t->directory[i].inode = t->directory[i+1].inode;
            strcpy(t->directory[i].fname, t->directory[i+1].fname);
        }
    }
    t->numEntries--;
    return 1;
}

// returns inode stored at entry
int directoryTable_getInode(directoryTable *t, int entry)
{
    return t->directory[entry].inode;
}

// copies name stored at entry into buffer
void directoryTable_getName(directoryTable *t, int entry, char* buffer)
{
    strcpy(buffer, t->directory[entry].fname);
}

// tries to find file with filename in one of directory entries
int directoryTable_findFile(directoryTable *t, char *fileName)
{
    char *tmp = (char *) malloc(sizeof(char) * (MAX_FILE_EXT_LEN + MAX_FILE_NAME_LEN + 2));
    for(int i = 0; i < MAX_FILES; i++)
    {
        if (t->directory[i].used == 1)
        {
            strcpy(tmp, t->directory[i].fname);
            int x = strcmp(tmp, fileName);
            if(x == 0)
            {
                free(tmp);
                return i;
            }
        }
    }
    free(tmp);
    return -1;
}
