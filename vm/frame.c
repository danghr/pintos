#include "vm/frame.h"
#include <stdlib.h>
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

static bool page_table_initialized = false;
static struct lock frame_table_lock;

/* Push frame table FTE to frame_table */
static void
frame_push_to_table (struct frame_table_entry *fte)
{
  ASSERT (!in_list (&(fte->elem), &frame_table));

  if (!lock_held_by_current_thread (&frame_table_lock))
    lock_acquire (&frame_table_lock);
  list_push_back (&frame_table, &(fte->elem));
  lock_release (&frame_table_lock);
}

/* Find the corresponding frame table according to the given PAGE. 
   Return NULL if not found. */
struct frame_table_entry *
frame_find_entry (void *page)
{
  for (struct list_elem *e = list_begin (&frame_table);
       e != list_end (&frame_table); e = list_next (e))
    {
      if (list_entry (e, struct frame_table_entry, elem)->frame == page)
        return list_entry (e, struct frame_table_entry, elem);
    }
  return NULL;
}

/* Initialize the frame table. */
void
frame_table_init ()
{
  if (page_table_initialized)
    return ;
  page_table_initialized = true;
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

/* Allocate a frame table according to given FLAGS.
   Return the address of the allocated page, or NULL if fails. */
void *
frame_allocate_page (enum palloc_flags flags)
{
  /* This function should ONLY be used to allocate frames from user pool 
     Refer to 4.1.5 (Really? ) */
  ASSERT (flags == PAL_USER || 
          flags == (PAL_ZERO | PAL_USER) ||
          flags == (PAL_ASSERT | PAL_USER) ||
          flags == (PAL_ZERO | PAL_ASSERT | PAL_USER));
  
  /* Try to allocate a page */
  uint8_t *frame = palloc_get_page (flags);

  /* If allocate success */
  if (frame != NULL)
    {
      /* Push into the list */
      struct frame_table_entry *fte = 
        malloc (sizeof (struct frame_table_entry));
      fte->owner = thread_current ();
      fte->frame = frame;
      frame_push_to_table (fte);
      return frame;
    }
  /* Need to implement evicting a frame according to LRU from 
     the current memory and reallocate the frame. */
  return NULL;
}

/* Free the given frame table entry FTE and the corresponding 
   page table. */
void
frame_free_fte (struct frame_table_entry *fte)
{
  if (!lock_held_by_current_thread (&frame_table_lock))
    lock_acquire (&frame_table_lock);
  list_remove (&(fte->elem));
  lock_release (&frame_table_lock);

  palloc_free_page (fte->frame);
  free (fte);
}

/* Free the given frame table entry according to the given PAGE */
void
frame_free_page (void *page)
{
  frame_free_fte (frame_find_entry (page));
}
