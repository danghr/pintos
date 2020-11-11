#include "vm/frame.h"
#include <stdlib.h>
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

/* List of all frame tables */
static struct lock frame_table_lock;

/* Push frame table FTE to frame_table_list */
static void *
push_frame_table_to_list (struct frame_table_entry *fte)
{
  ASSERT (!in_list (&(fte->elem), &frame_table));

  if (!lock_held_by_current_thread (&frame_table_lock))
    lock_acquire (&frame_table_lock);
  list_push_back (&frame_table, &(fte->elem));
  lock_release (&frame_table_lock);
}

/* Initialize the frame table */
void
frame_table_init ()
{
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

/* Allocate a frame table according to given FLAGS
   Return the address of the allocated page, or NULL if fails */
void *
frame_allocate_page (enum palloc_flags flags)
{
  /* Try to allocate a page */
  uint8_t *page = palloc_get_page (flags);

  /* If allocate success */
  if (page != NULL)
    {
      /* Push into the list */
      struct frame_table_entry *fte = 
        malloc (sizeof (struct frame_table_entry));
      fte->owner = thread_current ();
      fte->page = page;
      push_frame_table_to_list (fte);
      return page;
    }
  /* Need to implement evicting a page according to LRU from the current 
     memory and reallocate the page */
  return NULL;
}

/* Free the given frame table entry FTE and the corresponding page table */
void
frame_free_page (struct frame_table_entry *fte)
{
  if (!lock_held_by_current_thread (&frame_table_lock))
    lock_acquire (&frame_table_lock);
  list_remove (&(fte->elem));
  lock_release (&frame_table_lock);

  palloc_free_page (fte->page);
  free (fte);
}
