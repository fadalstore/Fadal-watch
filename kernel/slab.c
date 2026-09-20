#include "slab.h"

extern void *kmalloc(unsigned int bytes);
extern unsigned char kfree(void *address);

struct slab_cache process_slab_cache;
struct slab_cache fat12_dirent_slab_cache;
static slab_u8 slab_ready_flag;

static void zero_bytes(slab_u8 *target, slab_u32 count) {
    for (slab_u32 index = 0; index < count; index++) {
        target[index] = 0;
    }
}

static slab_u8 cache_init(struct slab_cache *cache, const char *name,
                          slab_u32 object_size, slab_u32 capacity) {
    cache->name = name;
    cache->object_size = object_size;
    cache->capacity = capacity;
    cache->storage = (slab_u8 *)kmalloc(object_size * capacity);
    if (cache->storage == (slab_u8 *)0) {
        return 0;
    }
    for (slab_u32 index = 0; index < SLAB_MAX_OBJECTS; index++) {
        cache->used[index] = 0;
    }
    zero_bytes(cache->storage, object_size * capacity);
    return 1;
}

void slab_system_init(void) {
    slab_ready_flag = cache_init(
        &process_slab_cache,
        "process",
        64,
        SLAB_PROCESS_CAPACITY);
    slab_ready_flag = slab_ready_flag && cache_init(
        &fat12_dirent_slab_cache,
        "fat12_dirent",
        32,
        SLAB_FAT12_DIRENT_CAPACITY);
}

void *slab_alloc(struct slab_cache *cache) {
    if (!slab_ready_flag || cache == (struct slab_cache *)0 ||
        cache->storage == (slab_u8 *)0) {
        return (void *)0;
    }
    for (slab_u32 index = 0; index < cache->capacity; index++) {
        if (cache->used[index] == 0) {
            cache->used[index] = 1;
            slab_u8 *object = cache->storage + index * cache->object_size;
            zero_bytes(object, cache->object_size);
            return object;
        }
    }
    return (void *)0;
}

slab_u8 slab_free(struct slab_cache *cache, void *object) {
    if (!slab_ready_flag || cache == (struct slab_cache *)0 ||
        object == (void *)0 || cache->storage == (slab_u8 *)0) {
        return 0;
    }
    slab_u32 address = (slab_u32)object;
    slab_u32 start = (slab_u32)cache->storage;
    slab_u32 end = start + cache->object_size * cache->capacity;
    if (address < start || address >= end ||
        ((address - start) % cache->object_size) != 0) {
        return 0;
    }
    slab_u32 index = (address - start) / cache->object_size;
    if (cache->used[index] == 0) {
        return 0;
    }
    cache->used[index] = 0;
    zero_bytes((slab_u8 *)object, cache->object_size);
    return 1;
}

slab_u32 slab_free_objects(const struct slab_cache *cache) {
    if (cache == (const struct slab_cache *)0) {
        return 0;
    }
    slab_u32 free_objects = 0;
    for (slab_u32 index = 0; index < cache->capacity; index++) {
        if (cache->used[index] == 0) {
            free_objects++;
        }
    }
    return free_objects;
}

slab_u8 slab_system_ready(void) {
    return slab_ready_flag;
}
