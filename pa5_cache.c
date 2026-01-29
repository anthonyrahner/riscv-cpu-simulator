#include "pa5_cache.h"

/*****************************************************************************
 *                      PROVIDED UTILITY FUNCTIONS                           *
 *****************************************************************************/
/*
 * checks whether x is a power of 2 (exculding zero)
 */
bool is_po2(uint64_t x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

/*
 * computes how many bits are required to represent the given unsigned number 
 */
int64_t nbits_required(uint64_t x)
{
    int64_t r = 0;
    while(x >>= 1)
        r++;
    return r;
}

/*****************************************************************************
 *                   RECOMMENDED UTILITY FUNCTIONS                           *
 *                                                                           *
 * We recommend you implement these to make the fetch() and memory() stage   *
 * cleaner. These will not be graded, but we provide some example assertion- *
 * based tests for them in `main.c`                                          *
 *****************************************************************************/
/*
 * calculates which byte position in a cache block the address points to (i.e.,
 * extracts the byte offset into the cache block)
 */
uint64_t get_byte_offset_within_cache_block(uint64_t address)
{

    //64 bytes per block = 6 bits for byte offset in block
    //16 sets = 4 bits for set index

    return address & ((1 << NUM_BITS_BYTE_OFFSET_IN_BLOCK) - 1); //lower 6 bits
    
}

/*
 * calculates which cache set the address belongs to (i.e., extracts the set
 * index field of the address)
 */
uint64_t get_cache_set_index(uint64_t address)
{

    //next 4 bits after byte offset
    return (address >> NUM_BITS_BYTE_OFFSET_IN_BLOCK) & ((1 << NUM_BITS_SET_INDEX) - 1);
}

/*
 * returns the cache block tag (i.e., extracts the tag field of the address)
 */
uint64_t get_cache_block_tag(uint64_t address)
{
    
    //tag = rest of the bits
    return address >> (NUM_BITS_BYTE_OFFSET_IN_BLOCK + NUM_BITS_SET_INDEX);

}

/*****************************************************************************
 *                   REQUIRED + GRADED FUNCTIONS                             *
 *****************************************************************************/
/*
 * copies a cache block's worth of data from main memory into the cache
 * 
 * inputs:
 *     block: the cache block to fill
 *     address: the memory address that the original ld/sd instruction attempted to access
 *     mem_read_byte: a function for reading one byte from memory at a specific address
 * assumptions:
 *     address will be a multiple of 8 bytes
 */
void cache_block_fill(struct cache_block *block, uint64_t address, uint8_t (*mem_read_byte)(uint64_t addr))
{
    
    //get the starting address of the cache block
    uint64_t block_address_start = address & ~((1ULL << NUM_BITS_BYTE_OFFSET_IN_BLOCK) - 1);

    for (int i = 0; i < CACHE_BLOCK_SIZE_BYTES; i++) //read bytes from memory and store it in cache block
    {
        block->data[i] = mem_read_byte(block_address_start + i);
    }

    //update tag, state = clean
    block->tag = get_cache_block_tag(address);
    block->state = STATE_CLEAN;
}

/*
 * copies a cache block's worth of data from the cache into main memory
 * 
 * inputs:
 *     block: the cache block to write back to memory
 *     address: the memory address that the original ld/sd instruction attempted to access
 *     mem_write_byte: a function for writing one byte to memory at a specific address
 * assumptions:
 *     address will be a multiple of 8 bytes
 */
void cache_block_writeback(struct cache_block *block, uint64_t address, void (*mem_write_byte)(uint64_t addr, uint8_t data))
{
    
    uint64_t block_address_start = address & ~((1ULL << NUM_BITS_BYTE_OFFSET_IN_BLOCK) - 1);

    //write bytes from block back to memory
    for (int i = 0; i < CACHE_BLOCK_SIZE_BYTES; i++)
    {
        mem_write_byte(block_address_start + i, block->data[i]);
    }

    //state = clean
    block->state = STATE_CLEAN;
}

/*
 * reads a dword (64 bits) from the cache, performing any necessary writeback or
 * fill operations to ensure that dirty data is not lost and that the correct
 * data is accessed.
 *
 * inputs:
 *     address: the address of the dword being read
 *     mem_read_byte: a function pointer to a function that allows reading a single byte from memory
 *     mem_write_byte: a function pointer to a function that allows writing a single byte to memory
 * outputs:
 *     out: the desired dword (either coming from cache or memory)
 * assumptions:
 *     address will be a multiple of 8 bytes
 */
uint64_t cache_read(uint64_t address, uint8_t (*mem_read_byte)(uint64_t addr), void (*mem_write_byte)(uint64_t addr, uint8_t data))
{

    //get all cache block info from address
    uint64_t byte_offset = get_byte_offset_within_cache_block(address);
    uint64_t set_index = get_cache_set_index(address);
    uint64_t tag = get_cache_block_tag(address);
    struct cache_block *block = &g_cache[set_index]; //initialize pointer to actual block

    //tag match and block has data = cache hit, return the data
    if (block->tag == tag && block->state != STATE_INVALID)
    {

        uint64_t block_data = 0; //uint64_t, little endian
        for (int i = 0; i < 8; i++)
        {
            block_data |= ((uint64_t)block->data[byte_offset + i]) << (8 * i);
        }
        return block_data;
    }

    if (block->state == STATE_DIRTY) //state = dirty, writeback to memory
    {
        cache_block_writeback(block, address, mem_write_byte);
    }

    cache_block_fill(block, address, mem_read_byte); //refill block with data from memory
    block->tag = tag;
    block->state = STATE_CLEAN;

    //now able to return data
    uint64_t block_data = 0;
    for (int i = 0; i < 8; i++)
    {
        block_data |= ((uint64_t)block->data[byte_offset + i]) << (8 * i);
    }

    return block_data;
    
}

/*
 * writes a dword (64 bits) to the cache, performing any necessary writeback or
 * fill operations to ensure that dirty data is not lost and that the correct
 * data is accessed.
 *
 * inputs:
 *     address: the address of the dword being written
 *     data: the dword to be written
 *     mem_read_byte: a function pointer to a function that allows reading a single byte from memory
 *     mem_write_byte: a function pointer to a function that allows writing a single byte to memory
 * assumptions: 
 *     address will be a multiple of 8 bytes
 */
void cache_write(uint64_t address, uint64_t data, uint8_t (*mem_read_byte)(uint64_t addr), void (*mem_write_byte)(uint64_t addr, uint8_t data))
{

    uint64_t byte_offset = get_byte_offset_within_cache_block(address);
    uint64_t set_index = get_cache_set_index(address);
    uint64_t tag = get_cache_block_tag(address);
    struct cache_block *block = &g_cache[set_index];

    //cache hit
    if (block->tag != tag && block->state == STATE_INVALID)
    {

        if (block->state == STATE_DIRTY) //state = dirty, write back
        {
            cache_block_writeback(block, address, mem_write_byte);
        }

        cache_block_fill(block, address, mem_read_byte); //fill block
        block->tag = tag;
        block->state = STATE_CLEAN;
    }

    for (int i = 0; i < 8; i++) //8 bytes, little endian into block
    {
        block->data[byte_offset + i] = (uint8_t)((data >> (8 * i)) & 0xFF);
    }

    //state now = dirty
    block->state = STATE_DIRTY;
}
