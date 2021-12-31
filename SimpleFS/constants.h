#define MAX_FILE_NAME_LEN       16
#define MAX_FILE_EXT_LEN        3

#define MAGIC                   0xABCD0005
#define BLOCKSIZE               1024
#define NUM_BLOCKS              1024

#define NUM_INDIR_PTRS          (BLOCKSIZE/4)
#define NUM_DIR_PTRS            12
#define DIR_PTR_BYTES           (NUM_DIR_PTRS * BLOCKSIZE)
#define MAX_FILE_SIZE           274432 //BLOCK_SIZE * MAX_NUM_FILE_BLOCKS
#define MAX_NUM_FILE_BLOCKS     268
#define MAX_FILES               50 //TODO: what should this be??

#define MAX_INODES              51
#define MAX_ICACHE_SIZE         4096
#define MAX_NUM_ICACHE_BLOCKS   4

#define MAX_NUM_DIREC_BLOCKS      4
