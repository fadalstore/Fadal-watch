#ifndef FADAL_SLAB_H
#define FADAL_SLAB_H

typedef unsigned char slab_u8;
typedef unsigned int slab_u32;

#define SLAB_PROCESS_CAPACITY 8
#define SLAB_FAT12_DIRENT_CAPACITY 16
#define SLAB_MAX_OBJECTS SLAB_FAT12_DIRENT_CAPACITY

struct slab_cache {
    const char *name;
    slab_u32 object_size;
    slab_u32 capacity;
    slab_u8 *storage;
    slab_u8 used[SLAB_MAX_OBJECTS];
};

extern struct slab_cache process_slab_cache;
extern struct slab_cache fat12_dirent_slab_cache;

void slab_system_init(void);
void *slab_alloc(struct slab_cache *cache);
slab_u8 slab_free(struct slab_cache *cache, void *object);
slab_u32 slab_free_objects(const struct slab_cache *cache);
slab_u8 slab_system_ready(void);

#endif
