#include <stdio.h>
#include <string.h>
#include <debug.h>
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/timer.h"

/* Size of buffer cache */
#define BUFFER_CACHE_SIZE 64
/* Period to flush all the cache into the disk */
#define BUFFER_CACHE_FLUSH_INTERVAL 20

/* Entries of buffer cache */
struct buffer_cache_entry
{
  bool using;                   /* Whether this sector is being used */

  /* Information of the cache */
  block_sector_t sector;        /* Sector on the disk of the cached file */
  bool dirty;                   /* Dirty bit */
  int64_t access_time;          /* Last access time */

  /* Data storage for a block */
  uint8_t buffer[BLOCK_SECTOR_SIZE];
};

/* Buffer cache entries */
static struct buffer_cache_entry buffer_cache[BUFFER_CACHE_SIZE];
/* Lock for buffer cache operations */
static struct lock buffer_cache_lock;
/* Flag that the buffer cache is initialzed */
bool buffer_cache_initialized = false;

/* Last time buffer cache flushed */
int64_t buffer_cache_last_flush = 30;
/* Last sector read, 0 for no need to read ahead */
block_sector_t buffer_cache_last_sector_loaded = 0;

/* Flush the given buffer cache entry ID */
static void
buffer_cache_flush (int to_evict)
{
  ASSERT (lock_held_by_current_thread (&buffer_cache_lock));
  ASSERT (0 <= to_evict && to_evict < BUFFER_CACHE_SIZE);

  struct buffer_cache_entry *bce = &buffer_cache[to_evict];
  ASSERT (bce->using);  /* Should not flush a non-using buffer */

  /* No need to write back if not dirty */
  if (bce->dirty == false)
    return ;
  
  block_write (fs_device, bce->sector, bce->buffer);
  bce->dirty = false;
}

/* Lookup the given buffer cache entry by the sector
   Returns the ID of the buffer cache entry, or -1 if not found */
static int
buffer_cache_lookup_sector (block_sector_t sector)
{
  ASSERT (lock_held_by_current_thread(&buffer_cache_lock));
  
  for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
    {
      if (!(buffer_cache[i].using))
        continue; /* Non-using cache should not be considered */
      if (buffer_cache[i].sector == sector)
        return i;
    }
  return -1;
}

/* Find an entry to evict */
static int
buffer_cache_evict (void)
{
  ASSERT (lock_held_by_current_thread (&buffer_cache_lock));

  /* Find cache to evict according to LRU */
  ASSERT (buffer_cache[0].using);
  int to_evict = 0;
  for (int i = 1; i < BUFFER_CACHE_SIZE; i++)
    {
      ASSERT (buffer_cache[i].using);
      if (buffer_cache[i].access_time < buffer_cache[to_evict].access_time)
        to_evict = i;
    }
  
  /* Evict the cache */
  buffer_cache_flush (to_evict);
  buffer_cache[to_evict].using = false;
  return to_evict;
}

/* Allocate an empty buffer cache 
   Returns the ID of the buffer cache */
static int
buffer_cache_allocate (void)
{
  ASSERT (lock_held_by_current_thread(&buffer_cache_lock));

  for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
    {
      /* If found an empty buffer cache, return it */
      if (!(buffer_cache[i].using))
        return i;
    }
  
  /* Otherwise evict a cache and return */
  return buffer_cache_evict ();
}

/* Load data from disk sector to the given buffer cache entry */
static void
buffer_cache_load (block_sector_t sector, struct buffer_cache_entry *bce)
{
  /* Copy the data in sectors to the cache */
  block_read (fs_device, sector, bce->buffer);
  /* Set the parameters */
  bce->dirty = false;
  bce->sector = sector;
  bce->using = true;
  bce->access_time = timer_ticks ();
  buffer_cache_last_sector_loaded = sector;
}

/* Initialize the buffer cache */
void
buffer_cache_init (void)
{
  lock_init (&buffer_cache_lock);

  for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
    buffer_cache[i].using = false;
  
  buffer_cache_initialized = true;
}

/* Flush all buffer caches */
void
buffer_cache_flush_all (void)
{
  /* Do nothing if not initialized */
  if (!buffer_cache_initialized)
    return ;
  
  lock_acquire (&buffer_cache_lock);

  for (int i = 0; i < BUFFER_CACHE_SIZE; i++)
      if (buffer_cache[i].using)
        buffer_cache_flush (i);

  lock_release (&buffer_cache_lock);
}

/* Periodically check the value */
void
buffer_cache_period (void *aux UNUSED)
{
  while (true)
  {
    /* Try to acquire the lock and then do the read ahead
       Do nothing if lock cannot be acquired, i.e. someone is operating
       on the buffer cache */
    if (buffer_cache_last_sector_loaded != 0)
      if (lock_try_acquire (&buffer_cache_lock))
        {
          /* Allocate a cache for read ahead */
          int cache_id = buffer_cache_allocate ();
          ASSERT (0 <= cache_id && cache_id < BUFFER_CACHE_SIZE);
          struct buffer_cache_entry *bce = &buffer_cache[cache_id];
          ASSERT (bce->using == false);

          /* Load data from the next disk sector */
          buffer_cache_load (buffer_cache_last_sector_loaded + 1, bce);
          /* No need to further load */
          buffer_cache_last_sector_loaded = 0;
          lock_release (&buffer_cache_lock);
        }

    /* Flush all every 20 timer ticks */
    if (timer_ticks () - buffer_cache_last_flush 
      >= BUFFER_CACHE_FLUSH_INTERVAL)
      {
        buffer_cache_flush_all ();
        buffer_cache_last_flush = timer_ticks ();
      }
    thread_yield ();
  }
}


/* Read/write operations through cache */

/* Read through cache */
void
buffer_cache_read (block_sector_t sector, void *memory)
{
  lock_acquire (&buffer_cache_lock);

  /* Find the corresponding buffer cache */
  int cache_id = buffer_cache_lookup_sector (sector);
  struct buffer_cache_entry *bce;

  /* Allocate a new one if not found */
  if (cache_id == -1)
    {
      cache_id = buffer_cache_allocate ();
      ASSERT (0 <= cache_id && cache_id < BUFFER_CACHE_SIZE);

      bce = &buffer_cache[cache_id];
      ASSERT (bce->using == false);

      /* Load data from disk sector */
      buffer_cache_load (sector, bce);
    }
  else
    bce = &buffer_cache[cache_id];

  /* Copy data to target memory */
  memcpy (memory, bce->buffer, BLOCK_SECTOR_SIZE);
  
  /* Set access time */
  bce->access_time = timer_ticks ();

  lock_release (&buffer_cache_lock);
}

/* Write through cache */
void
buffer_cache_write (block_sector_t sector, void *memory)
{
  lock_acquire (&buffer_cache_lock);

  /* Find the corresponding buffer cache */
  int cache_id = buffer_cache_lookup_sector (sector);
  struct buffer_cache_entry *bce;

  /* Allocate a new one if not found */
  if (cache_id == -1)
    {
      cache_id = buffer_cache_allocate ();
      ASSERT (0 <= cache_id && cache_id < BUFFER_CACHE_SIZE);

      bce = &buffer_cache[cache_id];
      ASSERT (bce->using == false);

      /* Load data from disk sector */
      buffer_cache_load (sector, bce);
    }
  else
    bce = &buffer_cache[cache_id];

  /* Copy data to target memory */
  memcpy (bce->buffer, memory, BLOCK_SECTOR_SIZE);
  bce->dirty = true;
  
  /* Set access time */
  bce->access_time = timer_ticks ();

  lock_release (&buffer_cache_lock);
}
