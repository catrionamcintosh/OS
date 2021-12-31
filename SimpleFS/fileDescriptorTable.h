#include "constants.h"
struct fileDescriptorTableEntry
{
    int rwPtr;
    int opened;
} typedef fileDescriptorTableEntry;

struct fileDescriptorTable
{
    fileDescriptorTableEntry fdt[MAX_FILES];
} typedef fileDescriptorTable;

// initialize values in fileDescriptorTable
void create_fdt(fileDescriptorTable *table)
{
    for(int i = 0; i < MAX_FILES; i++)
    {
        table->fdt[i].rwPtr = 0;
        table->fdt[i].opened = 0;
    }
}

void fdt_addEntry(fileDescriptorTable *t, int fd, int ptr)
{
    t->fdt[fd].rwPtr = ptr;
    t->fdt[fd].opened = 1;
}

int get_rwPtr(fileDescriptorTable *t, int i)
{
    return t->fdt[i].rwPtr;
}

void set_rwPtr(fileDescriptorTable *t, int i, int newVal)
{
    t->fdt[i].rwPtr = newVal;
}

int fdt_closeEntry(fileDescriptorTable *t, int fd)
{
    // if already closed return -1
    if(t->fdt[fd].opened == 0)
    {
        return -1;
    }
    
    t->fdt[fd].opened = 0;
    return 1;
}

int fdt_removeEntry(fileDescriptorTable *t, int fd)
{
    //TODO: Error handle
    t->fdt[fd].opened = 0;
    t->fdt[fd].rwPtr = 0;

    return 1;
}

int fdt_entryStatus(fileDescriptorTable *t, int fd)
{
    return t->fdt[fd].opened;
}
