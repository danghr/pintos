#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Number of direct blocks in an inode. */
#define DIRECT_BLOCK 12
/* Number of indirect blocks stored in a sector. */
#define INDIRECT_BLOCK \
  ((BLOCK_SECTOR_SIZE) / (sizeof (block_sector_t)))
/* Maximum number of sectors in an inode */
#define MAXIMUM_SECTORS_IN_INODE \
  ((DIRECT_BLOCK) + (INDIRECT_BLOCK) + \
   (INDIRECT_BLOCK) * (INDIRECT_BLOCK))

/* Return minimum. */
#define min(a, b) ((a < b) ? (a) : (b))

/* inode operation lock. */
struct lock inode_extension_lock;

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    /* Direct and indirect blocks. 
       - DIRECT_BLOCK blocks
       - 1 indirect block
       - 1 double indirect block */
    block_sector_t blocks[DIRECT_BLOCK + 2];
    
    /* inode metadata */
    off_t length;                             /* File size in bytes. */
    unsigned magic;                           /* Magic number. */

    /* MODIFY THE FOLLOWING IF VARIABLES IN THIS STRUCTURE ARE MODIFIED */
    /* To meet BLOCK_SECTOR_SIZE size requirement. */
    char unused[BLOCK_SECTOR_SIZE
                - sizeof (block_sector_t) * (DIRECT_BLOCK + 2)  /* blocks */
                - sizeof (off_t)              /* length */
                - sizeof (unsigned)           /* magic */
               ];
  };

/* Indirect blocks stored in a sector. */
struct inode_indirect_block_sector
  {
    /* Sectors */
    block_sector_t blocks[INDIRECT_BLOCK];
  };

/* Double indirect blocks stored in a sector. */
struct inode_double_indirect_block_sector
  {
    /* Sectors */
    block_sector_t indirect_blocks[INDIRECT_BLOCK];
  };

/* Three levels of blocks */
static int
sector_calc_level (off_t index)
{
  ASSERT (index >= 0);
  ASSERT (index < (int)(MAXIMUM_SECTORS_IN_INODE));

  if (index < (int)(DIRECT_BLOCK))
    return 1;
  else if (index < (int)(DIRECT_BLOCK + INDIRECT_BLOCK))
    return 2;
  else
    return 3;
}

/* Returns the block device sector that is the position INDEX of
   the inode_disk IDISK.
   Returns -1 if not found. */
static block_sector_t
index_to_sector (const struct inode_disk *idisk, off_t index)
{
  ASSERT (idisk != NULL);
  ASSERT (index >= 0);
  ASSERT (index < (int)(MAXIMUM_SECTORS_IN_INODE));

  /* Return value */
  block_sector_t ret;

  /* Memory to store the indirect block read from the disk */
  struct inode_indirect_block_sector *iibs;
  struct inode_double_indirect_block_sector *idibs;

  /* Calculate sector level */
  int sector_level = sector_calc_level (index);

  /* Situation 1: If the index is in the direct block. */
  if (sector_level == 1)
    return idisk->blocks[index];

  /* Situation 2: If the index is in the indirect block. */
  if (sector_level == 2)
    {
      /* Read the data from the indirect block sector. */
      iibs = malloc (sizeof (struct inode_indirect_block_sector));
      buffer_cache_read (idisk->blocks[DIRECT_BLOCK], iibs);

      /* Fetch the sector to return. */
      ret = iibs->blocks[index - DIRECT_BLOCK];

      /* Free allocated memory and return. */
      free (iibs);
      return ret;
    }

  /* Situation 3: If the index is in the double indirect block. */
  if (sector_level == 3)
    {
      /* Calculate the first and double level index. */
      off_t index1 = 
        (index - DIRECT_BLOCK - INDIRECT_BLOCK) / INDIRECT_BLOCK;
      off_t index2 = 
        (index - DIRECT_BLOCK - INDIRECT_BLOCK) % INDIRECT_BLOCK;
      ASSERT (index1 < (off_t)(INDIRECT_BLOCK));
      ASSERT (index2 < (off_t)(INDIRECT_BLOCK));

      /* Read data from double indirect block sector from the disk. */
      idibs = malloc (sizeof (struct inode_double_indirect_block_sector));
      buffer_cache_read (idisk->blocks[DIRECT_BLOCK + 1], idibs);

      /* Read data from the second-level indirect block sector. */
      iibs = malloc (sizeof (struct inode_indirect_block_sector));
      buffer_cache_read (idibs->indirect_blocks[index1], iibs);

      /* Fetch the sector to return. */
      ret = iibs->blocks[index2];

      /* Free allocated memory and return. */
      free (idibs);
      free (iibs);
      return ret;
    }
  
  /* Otherwise return -1. */
  return (block_sector_t)(-1);
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the sector index of a given byte SIZE. */
static inline off_t
bytes_to_index (off_t size)
{
  return size / BLOCK_SECTOR_SIZE;
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return index_to_sector (&(inode->data), bytes_to_index (pos));
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  lock_init (&inode_extension_lock);
  list_init (&open_inodes);
}

/* Allocate (or extend) sectors for indirect block IBLOCK so that
   it can contain SECTOR_CNT sectors. Sectors already allocated
   are not modified. 
   Returns true if succeeds, false otherwise. */
static bool
inode_indirect_allocate (block_sector_t iblock, size_t sector_cnt)
{
  ASSERT (iblock > 0);
  ASSERT (sector_cnt <= INDIRECT_BLOCK);

  /* Zero bytes to write. */
  static char zeros[BLOCK_SECTOR_SIZE];

  /* Read indirect block data from block sector. */
  struct inode_indirect_block_sector *iibs;
  iibs = malloc (sizeof (struct inode_indirect_block_sector));
  buffer_cache_read (iblock, iibs);

  /* Allocate sectors and write to the disk. */
  for (unsigned int i = 0; i < sector_cnt; i++)
    {
      /* Skip if the sector is already allocated and allocate sector 
         for unallocated blocks. */
      if (iibs->blocks[i] == 0)
        {
          /* Allocate sector. */
          if (!free_map_allocate (1, &(iibs->blocks[i])))
            goto fail;
          /* Write all zeroes. */
          buffer_cache_write (iibs->blocks[i], &zeros);
        }
    }
  
  /* Write the information back to the disk. */
  buffer_cache_write (iblock, iibs);

  free (iibs);
  return true;

fail:
  free (iibs);
  return false;
}

/* Allocate (or extend) sectors for double indirect block IBLOCK so
   that it can contain SECTOR_CNT sectors. Sectors already allocated
   are not modified. 
   Returns true if succeeds, false otherwise. */
static bool
inode_double_indirect_allocate (block_sector_t iblock, size_t sector_cnt)
{
  ASSERT (iblock > 0);
  ASSERT (sector_cnt <= INDIRECT_BLOCK * INDIRECT_BLOCK);
  
  /* Read indirect block data from block sector. */
  struct inode_double_indirect_block_sector *idibs;
  idibs = malloc (sizeof (struct inode_double_indirect_block_sector));
  buffer_cache_read (iblock, idibs);

  /* Remaining sectors to allocate. */
  size_t remaining_sectors = sector_cnt;
  /* Sectors to allocate in this loop. */
  size_t to_allocate_sectors;
  /* Loop count. */
  int i = 0;

  /* Allocate sectors. */
  while (remaining_sectors > 0)
    {
      /* If indirect block does not exist then allocate one. */
      if (idibs->indirect_blocks[i] == 0)
        {
          /* Allocate sector to store sectors of indirect blocks. */
          if (!free_map_allocate (1, &(idibs->indirect_blocks[i])))
            goto fail;
        }

      /* Calculate indirect blocks to allocate in this loop. */
      to_allocate_sectors = 
        min (remaining_sectors, INDIRECT_BLOCK);

      /* Allocate indirect blocks. */
      inode_indirect_allocate 
        (idibs->indirect_blocks[i], to_allocate_sectors);

      /* Deduct allocated blocks. */
      remaining_sectors -= to_allocate_sectors;
      i++;
    }
  
  /* Write the information back to the disk. */
  buffer_cache_write (iblock, idibs);

  free (idibs);
  return true;

fail:
  free (idibs);
  return false;
}

/* Allocate (or extend) sectors for inode IDISK so that it can
   contain file with SIZE bytes. Sectors already allocated are
   not modified. 
   Returns true if succeeds, false otherwise. */
static bool
inode_allocate (struct inode_disk *idisk, off_t size)
{
  ASSERT (idisk != NULL);
  ASSERT (size >= 0);
  ASSERT (size < BLOCK_SECTOR_SIZE * MAXIMUM_SECTORS_IN_INODE);

  /* Zero bytes to write. */
  static char zeros[BLOCK_SECTOR_SIZE];

  /* Calculate the number of total sectors needed. */
  size_t total_sector_cnt = bytes_to_sectors (size);
  /* Remaining sectors to allocate. */
  size_t remaining_sectors = total_sector_cnt;
  
  /* Part 1: Allocate the part in the direct block. */
  size_t sectors_to_allocate_direct = 
    min (remaining_sectors, DIRECT_BLOCK);
  for (unsigned int i = 0; i < sectors_to_allocate_direct; i++)
    {
      /* Skip if the sector is already allocated and allocate sector 
         for unallocated blocks. */
      if (idisk->blocks[i] == 0)
        {
          if (!free_map_allocate (1, &(idisk->blocks[i])))
            return false;
          /* Write all zeroes if allocate success. */
          buffer_cache_write (idisk->blocks[i], &zeros);
        }
    }
  remaining_sectors -= sectors_to_allocate_direct;

  /* Part 2: Allocate the part in the indirect block. */
  size_t sectors_to_allocate_indirect = 
    min (remaining_sectors, INDIRECT_BLOCK);
  if (sectors_to_allocate_indirect > 0)
    {
      /* Allocate sector to store indirect block if not yet 
         allocated. */
      if (idisk->blocks[DIRECT_BLOCK] == 0)
        if (!free_map_allocate (1, &(idisk->blocks[DIRECT_BLOCK])))
          return false;
      
      /* Allocate sectors for indirect blocks. */
      if (!inode_indirect_allocate 
        (idisk->blocks[DIRECT_BLOCK], sectors_to_allocate_indirect))
        return false;
    }
  remaining_sectors -= sectors_to_allocate_indirect;

  /* Part 3: The part in the double indirect block. */
  size_t sectors_to_allocate_double_indirect = 
    min (remaining_sectors, INDIRECT_BLOCK * INDIRECT_BLOCK);
  if (sectors_to_allocate_double_indirect > 0)
    {
      /* Allocate sector to store double indirect block if 
         not yet allocated. */
      if (idisk->blocks[DIRECT_BLOCK + 1] == 0)
        if (!free_map_allocate (1, &(idisk->blocks[DIRECT_BLOCK + 1])))
          return false;
      
      /* Allocate sectors for double indirect blocks. */
      if (!inode_double_indirect_allocate 
        (idisk->blocks[DIRECT_BLOCK + 1], 
        sectors_to_allocate_double_indirect))
        return false;
    }
  remaining_sectors -= sectors_to_allocate_double_indirect;

  /* Check whether all sectors are allocated. */
  if (remaining_sectors == 0)
    return true;
  return false;
}

/* Free all sectors contained in indirect block IBLOCK. */
static void
inode_indirect_free (block_sector_t iblock)
{
  ASSERT (iblock > 0);

  /* Read indirect block data from block sector. */
  struct inode_indirect_block_sector *iibs;
  iibs = malloc (sizeof (struct inode_indirect_block_sector));
  buffer_cache_read (iblock, iibs);

  /* Free all the blocks. */
  for (unsigned int i = 0; i < INDIRECT_BLOCK; i++)
    if (iibs->blocks[i] != 0)
      free_map_release (iibs->blocks[i], 1);

  /* Free this sector. */
  free_map_release (iblock, 1);

  free (iibs);
}

/* Free all sectors contained in double indirect block IBLOCK. */
static void
inode_double_indirect_free (block_sector_t iblock)
{
  ASSERT (iblock > 0);

  /* Read indirect block data from block sector. */
  struct inode_double_indirect_block_sector *idibs;
  idibs = malloc (sizeof (struct inode_double_indirect_block_sector));
  buffer_cache_read (iblock, idibs);

  /* Free all the indirect blocks. */
  for (unsigned int i = 0; i < INDIRECT_BLOCK; i++)
    if (idibs->indirect_blocks[i] != 0)
      inode_indirect_free (idibs->indirect_blocks[i]);

  /* Free this sector. */
  free_map_release (iblock, 1);

  free (idibs);
}

/* Free all the sectors for inode IDISK. */
static void
inode_free (struct inode_disk *idisk)
{
  ASSERT (idisk != NULL);
  
  /* Free all direct blocks. */
  for (int i = 0; i < DIRECT_BLOCK; i++)
    if (idisk->blocks[i] != 0)
      free_map_release (idisk->blocks[i], 1);

  /* Free all indirect blocks. */
  if (idisk->blocks[DIRECT_BLOCK] != 0)
    inode_indirect_free (idisk->blocks[DIRECT_BLOCK]);

  /* Free all double indirect blocks. */
  if (idisk->blocks[DIRECT_BLOCK + 1] != 0)
    inode_double_indirect_free (idisk->blocks[DIRECT_BLOCK + 1]);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      /* Try to allocate space for the given length. */
      if (inode_allocate (disk_inode, length))
        {
          /* Write the new inode to the disk. */
          buffer_cache_write (sector, disk_inode);
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  buffer_cache_read (inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          /* Free the sector of this inode */
          free_map_release (inode->sector, 1);
          /* Free all allocated sectors. */
          inode_free (&(inode->data));
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      /* Stop reading if cannot find */
      if (sector_idx == (block_sector_t)(-1))
        break;
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          buffer_cache_read (sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          buffer_cache_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs. */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  /* Extend file if write after EOF, i.e. cannot find sector in inode. */
  /* Last byte to write: OFFSET + SIZE - 1
     Refer to the comment of this function. */
  if (byte_to_sector (inode, offset + size - 1) == (block_sector_t)(-1))
    {
      /* Acquire the lock. */
      lock_acquire (&inode_extension_lock);

      /* Check again */
      if (byte_to_sector (inode, offset + size - 1) == (block_sector_t)(-1))
        {
          /* Allocate enough space of file. */
          if (!inode_allocate (&(inode->data), offset + size))
            {
              lock_release (&inode_extension_lock);
              return 0;
            }

          /* Update file metadata. */
          inode->data.length = offset + size;

          /* Write the updated inode to the disk. */
          buffer_cache_write (inode->sector, &(inode->data));
        }

      /* Release the lock */
      lock_release (&inode_extension_lock);
    }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          buffer_cache_write (sector_idx, (void*) (buffer + bytes_written));
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            buffer_cache_read (sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          buffer_cache_write (sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
