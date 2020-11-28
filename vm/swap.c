#include <bitmap.h>
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "devices/block.h"

static struct bitmap *swap_bitmap;
static struct block *swap_block;
static size_t swap_size;

/* Intialize the swap_block and swap_bitmap. */
void
swap_init ()
{
  swap_block = block_get_role (BLOCK_SWAP);
  if (swap_block == NULL)
    PANIC ("Cannot initialize the swap block");

  /* Calculate the size of the bitmap */
  swap_size = block_size (swap_block) / (PGSIZE / BLOCK_SECTOR_SIZE);
  swap_bitmap = bitmap_create (swap_size);
  /* Set all the entry to TRUE, when we store sth in the swap block we will set it false. */
  bitmap_set_all (swap_bitmap, true);
}

/* Store a page into the swap slot given the address of the page*/
size_t
store_in_swap (void *page)
{
  /* We can just store the uservaddr in the swap. */
  ASSERT (page > PHYS_BASE);

  /* Find the free swap slot. */
  size_t swap_index = bitmap_scan (swap_bitmap, 0, 1, true);
  if (swap_index == BITMAP_ERROR)
    PANIC ("no place in the swap.");
  
  for (size_t i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
      /* Store the information in the swap_block. */
      block_write (swap_block,
        swap_index * (PGSIZE / BLOCK_SECTOR_SIZE) + i,
        page + (PGSIZE / BLOCK_SECTOR_SIZE) * i);
    }
  bitmap_set (swap_bitmap, swap_index, false);
  return swap_index;
}

/* Extract the page from the swap slot given index. */
void
read_from_swap (size_t index, void *page)
{
  /* The index must smaller than the swap_size. */
  ASSERT (index < swap_size);
  /* We can just store in the uservaddr. */
  ASSERT (page > PHYS_BASE)

  bool is_in_swap = !bitmap_test (swap_bitmap, index);
  if (is_in_swap)
    {
      for (size_t i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
        {
          block_read (swap_block,
          index * (PGSIZE / BLOCK_SECTOR_SIZE) + i,
          page + (PGSIZE / BLOCK_SECTOR_SIZE) * i);
        }
      bitmap_set (swap_bitmap, index, true);
    }
  else
    PANIC ("READ FROM AN EMPTY SWAP SLOT");
}

/* Free the corresponding swap slot. */
void swap_free (size_t index)
{
  ASSERT (index < swap_size);
  bool is_in_swap = !bitmap_test (swap_bitmap, index);
  if (is_in_swap)
    bitmap_set (swap_bitmap, index, true);
  else
    PANIC ("FREE AN EMPTY SWAP SLOT");
}
