#include "cache.h"
#include <iostream>
#include <tuple>
#include <math.h>

class Cache
{
private:
    /* data */
    cache_params_t params;

    unsigned pref_en;
    // for cache
    cache_block **cache; // 2-D array  the pointer to a bunch of pointer (cache_block we defined above)
    uint32_t number_set;
    uint32_t INDEX_BITS;
    uint32_t BLOCKOFFSET_BITS;
    uint32_t tag;
    uint32_t index;

    uint32_t cache_size;
    uint32_t assoc;

    uint32_t evicted_old_addr;
    uint32_t evicted_block;

public:
    Cache(cache_params_t params, bool L1_or_L2_);
    ~Cache();
    Evict_or_not request(char rw, uint32_t addr);
    Evict_or_not request_no_pref(char rw, uint32_t addr);
    Evict_or_not request_pref(char rw, uint32_t addr);
    void update_LRU(char rw, uint32_t index, uint32_t L1_hit_blk_offset_LRU, uint32_t hit_blk_offset);
    Flag_Addr search_block(uint32_t index, uint32_t tag);
    Dirty_Addr evict_block(uint32_t index);
    Flag_Addr find_empty_block(uint32_t index);
    void insert_block(char rw, uint32_t index, uint32_t block, uint32_t tag);
    void print_result();
    void print_content();

    // for count L1
    unsigned cnt_Write;
    unsigned cnt_Write_miss;
    unsigned cnt_Read;
    unsigned cnt_Read_miss;
    unsigned cnt_writebacks;
    bool L1_or_L2;

    static int cal(int a, int b) { return a + b; }
};

/*Print out the given mem address structure*/
Cache::Cache(cache_params_t _params, bool L1_or_L2_)
{
    params = _params;
    assoc = (L1_or_L2_) ? _params.L2_ASSOC : _params.L1_ASSOC;

    cache_size = (L1_or_L2_) ? _params.L2_SIZE : _params.L1_SIZE;
    // std::cout << " cache_size=" << cache_size << "\n";

    pref_en = 0;
    cnt_Write = cnt_Write_miss = cnt_Read = cnt_Read_miss = cnt_writebacks = 0;
    // std::cout << " ASSOC=" << assoc << "\n";
    /*cahce caluation set*/
    number_set = cache_size / (params.BLOCKSIZE * assoc);
    INDEX_BITS = log2(number_set);
    BLOCKOFFSET_BITS = log2(params.BLOCKSIZE);

    /*create cache_block*/
    cache = new cache_block *[number_set];    // take a space can put cache_block*[number_set] to cache
    for (uint32_t i = 0; i < number_set; i++) // this for make it become a 2-d array
    {
        cache[i] = new cache_block[assoc]; // cache is the index of the address last ta key" cache[i] = new cache_block[assoc];"

        for (uint32_t j = 0; j < assoc; j++)
        { // initialize lru,valid,dirty bit
            cache[i][j].lru = j;
            cache[i][j].valid = 0;
            cache[i][j].dirty = 0;
        }
    }
}

Cache::~Cache()
{
}
/* return: {cache_status_msg, addr} */
Evict_or_not Cache::request(char rw, uint32_t addr)
{
    // if (pref_en)
    //     request_pref(rw, addr);
    // else
    return request_no_pref(rw, addr);
    // else into this function
}

// return: {cache_status_msg, addr}
Evict_or_not Cache::request_no_pref(char rw, uint32_t addr)
{
    //{tag, index, blockoffset}
    tag = addr >> (INDEX_BITS + BLOCKOFFSET_BITS);   // tag from address
    uint32_t BIT_MASK = (1 << (INDEX_BITS)) - 1;     // bit mask = {INDEX_BITS{1}};
    uint32_t index_tag = addr >> (BLOCKOFFSET_BITS); // process
    index = index_tag & BIT_MASK;
    // printf("tag %x\t, index %x\t, INDEX_BITS=%d\t, BLOCKOFFSET_BITS=%d\n", tag, index, INDEX_BITS, BLOCKOFFSET_BITS);

    bool hit = false;
    bool hasEmpty = false;

    uint32_t hit_blk_offset;

    dirty_t dirty = CLEAN;
    uint32_t block_evicted;

    std::tie(hit, hit_blk_offset) = search_block(index, tag);

    if (hit)
    {
        update_LRU(rw, index, cache[index][hit_blk_offset].lru, hit_blk_offset); // "hit" lru update
    }
    else
    {
        // printf("miss \n");
        uint32_t empty_block;

        std::tie(hasEmpty, empty_block) = find_empty_block(index); //

        if (hasEmpty)
        {

            insert_block(rw, index, empty_block, tag);
            // return std::make_tuple(MISS, evicted_old_addr);
        }
        else
        {

            std::tie(dirty, block_evicted) = evict_block(index);

            uint32_t evicted_old_addr_tmp = cache[index][block_evicted].tag << (INDEX_BITS + BLOCKOFFSET_BITS);
            evicted_old_addr = evicted_old_addr_tmp + ((index << BLOCKOFFSET_BITS) + block_evicted);

            tag = addr >> (INDEX_BITS + BLOCKOFFSET_BITS);   // tag from address
            uint32_t BIT_MASK = (1 << (INDEX_BITS)) - 1;     // bit mask = {INDEX_BITS{1}};
            uint32_t index_tag = addr >> (BLOCKOFFSET_BITS); // process
            index = index_tag & BIT_MASK;

            insert_block(rw, index, block_evicted, tag);
            // return std::make_tuple(EVICT, evicted_old_addr);
        }
    }
    if (rw == 'w')
        cnt_Write++;
    if (rw == 'w' && !hit)
        cnt_Write_miss++;
    if (rw == 'r')
        cnt_Read++;
    if (rw == 'r' && !hit)
        cnt_Read_miss++;
    if (dirty == DIRTY)
    {
        cnt_writebacks++;
    }

    if (hit)
        return std::make_tuple(HIT, 0);
    else
    {
        if (hasEmpty)
        {
            return std::make_tuple(MISS, 0);
        }
        else if (dirty == DIRTY)
        {
            return std::make_tuple(EVICT, evicted_old_addr);
        }
        else
        {
            return std::make_tuple(MISS, 0);
        }
    }
    // if write back
    // if(dirty)
}

// return : hit, block
Flag_Addr Cache::search_block(uint32_t index, uint32_t tag)
{
    //  std::cout << " in search, index " << index << " tag " << tag << "\n";
    for (uint32_t j = 0; j < assoc; j++)
    {
        // std::cout << " assoc " << j << "tag " << cache[index][j].tag << " valid " << cache[index][j].valid << " \n";
        if (cache[index][j].tag == tag && cache[index][j].valid == true)
        {
            // std::cout << "hit" << std::endl;
            return std::make_tuple(true, j);
            // cache[index][j].lru = 0;
            // update_LRU(cache[index], cache[index][j].lru, j);
        } // end if
    }     // end for assoc
    return std::make_tuple(false, 9999);
}

/* already get the hit block then update others in index*/
void Cache::update_LRU(char rw, uint32_t index, uint32_t L1_hit_blk_offset_LRU, uint32_t hit_blk_offset)
{

    if (rw == 'w')
        cache[index][hit_blk_offset].dirty = 1;
    if (cache[index][hit_blk_offset].lru == 0)
        return;

    for (uint32_t i = 0; i < assoc; i++)
    {
        if (cache[index][i].lru <= L1_hit_blk_offset_LRU)
            cache[index][i].lru++;
    }

    cache[index][hit_blk_offset].lru = 0;
}

Dirty_Addr Cache::evict_block(uint32_t index)
{
    uint32_t max_lru = assoc - 1;
    // std::cout << " ASSOC=" << assoc << "\n";
    for (uint32_t j = 0; j < assoc; j++)
    {
        //  std::cout << " assoc " << j << " lru " << cache[index][j].lru << "max_lru " << max_lru << " \n";
        if (cache[index][j].lru == max_lru)
        {
            if (cache[index][j].dirty == 1)
                return std::make_tuple(DIRTY, j); // name the j asociative as block that evicted

            else
                return std::make_tuple(CLEAN, j);
        }
    }
    std::cout << "can't find evict error\n";
    exit(0);
}

// return: hasEmpty, empty_block
Flag_Addr Cache::find_empty_block(uint32_t index)
{
    for (uint32_t j = 0; j < assoc; j++)
    {
        if (cache[index][j].valid == 0) // valid ==0 means, that index's space is empty
            return std::make_tuple(true, j);
    }
    return std::make_tuple(false, 999999);
}

void Cache::insert_block(char rw, uint32_t index, uint32_t block_offset, uint32_t tag)
{
    cache[index][block_offset].tag = tag;
    cache[index][block_offset].valid = true;
    cache[index][block_offset].dirty = (rw == 'w') ? 1 : 0; // clean dirty if insert a read block!! otherwise will have unnormal dirty blocks
    update_LRU(rw, index, cache[index][block_offset].lru, block_offset);
}

void Cache::print_content()
{
    for (uint32_t i = 0; i < number_set; i++)
    {
        std::cout << " set " << i << ":   ";

        for (uint32_t lru_num = 0; lru_num < assoc; lru_num++)
        {
            for (uint32_t j = 0; j < assoc; j++)
            {
                if (cache[i][j].lru == lru_num)
                {
                    printf("%x ", cache[i][j].tag);
                    if (cache[i][j].dirty == 1)
                        printf(" D ");
                    else
                        printf("  ");
                }
            }
        }
        printf(" \n");
    }
}

