#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"

void buffer_cache_init (void);
void buffer_cache_flush_all (void);

void buffer_cache_read (block_sector_t, void *);
void buffer_cache_write (block_sector_t, void *);

void buffer_cache_period (void *);

#endif /* filesys/cache.h */
