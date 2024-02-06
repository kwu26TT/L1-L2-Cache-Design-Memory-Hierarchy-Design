#include "sim.h"
#include <tuple>
#include <math.h>

typedef enum
{
   CLEAN = 0,
   DIRTY = 1
} dirty_t;

typedef struct cache_block
{
   bool valid;
   bool dirty;
   uint32_t tag;
   uint32_t lru;

} cache_block;

typedef enum
{
   HIT = 0,
   MISS = 1,
   EVICT = 2,

} cache_status_msg;

typedef std::tuple<bool, uint32_t> Flag_Addr;

typedef std::tuple<dirty_t, uint32_t> Dirty_Addr;

typedef std::tuple<cache_status_msg, uint32_t> Evict_or_not;