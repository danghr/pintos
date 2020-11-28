#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include <stdlib.h>
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"

/* Page initialition flag */
static bool page_table_initialized = false;
/* Lock of frame table */
static struct lock frame_table_lock;

/* Push frame table FTE to frame_table */
static void
frame_push_to_table (struct frame_table_entry *fte)
{
  ASSERT (lock_held_by_current_thread (&frame_table_lock));
  ASSERT (!in_list (&(fte->elem), &frame_table));

  list_push_back (&frame_table, &(fte->elem));
}

/* Find the corresponding frame table according to the given PAGE. 
   Return NULL if not found. */
struct frame_table_entry *
frame_find_entry (void *addr)
{
  for (struct list_elem *e = list_begin (&frame_table);
       e != list_end (&frame_table); e = list_next (e))
    {
      if (list_entry (e, struct frame_table_entry, elem)->frame == addr)
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

/* When the memory is full, we need to implement the eviction. */
struct frame_table_entry *
find_entry_to_evict()
{
  list_sort(&frame_table,(list_less_func *)compare_access_time, NULL);
  if(list_front(&frame_table) != NULL)
    return list_entry 
      (list_front (&frame_table), struct frame_table_entry, elem);
  PANIC("DO not need to evict");
}

bool compare_access_time(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  return list_entry(a,struct frame_table_entry,elem)->spte->access_time 
          < list_entry(b,struct frame_table_entry,elem)->spte->access_time;
}
/* Allocate a frame table according to given FLAGS.
   Return the frame table entry of the allocated page, or NULL if fails. */
struct frame_table_entry *
frame_allocate_page 
  (struct sup_page_table_entry *spte, enum palloc_flags flags)
{
  /* This function should ONLY be used to allocate frames from user pool 
     Refer to 4.1.5 */
  ASSERT (flags == PAL_USER || 
          flags == (PAL_ZERO | PAL_USER) ||
          flags == (PAL_ASSERT | PAL_USER) ||
          flags == (PAL_ZERO | PAL_ASSERT | PAL_USER));
  
  bool lock_flag = false;
  if (!lock_held_by_current_thread (&frame_table_lock))
    {
      lock_acquire (&frame_table_lock);
      lock_flag = true;
    }
  
  /* Try to allocate a page */
  void *f = palloc_get_page (flags);
  /* If allocate not success */
  if (f == NULL)
    {
      struct frame_table_entry *fte_to_evict = find_entry_to_evict ();
      struct sup_page_table_entry *spte_correspond = fte_to_evict->spte;
      size_t swap_index = store_in_swap (fte_to_evict->frame);
      spte_correspond->status = IN_SWAP;
      spte_correspond->swap_index = swap_index;
      
      frame_free_fte (fte_to_evict);
      f = palloc_get_page (flags);
      
      if (f == NULL)
        return NULL;
    }
  struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));
  fte->frame = f;
  fte->spte = spte;
  frame_push_to_table (fte);
  
  if (lock_held_by_current_thread (&frame_table_lock) && lock_flag)
    lock_release (&frame_table_lock);
  return fte;
}

/* Free the given frame table entry FTE and the corresponding 
   page table. */
void
frame_free_fte (struct frame_table_entry *fte)
{
  bool lock_flag = false;
  if (!lock_held_by_current_thread (&frame_table_lock))
    {
      lock_acquire (&frame_table_lock);
      lock_flag = true;
    }
  pagedir_clear_page (fte->spte->owner->pagedir, fte->spte->user_vaddr);
  if (!lock_held_by_current_thread (&frame_table_lock))
    lock_acquire (&frame_table_lock);
  list_remove (&(fte->elem));
  lock_release (&frame_table_lock);
  palloc_free_page (fte->frame);
  fte->spte->fte = NULL;
  free (fte);
  if (lock_held_by_current_thread (&frame_table_lock) && lock_flag)
    lock_release (&frame_table_lock);
}

/* Free the given frame table entry according to the given PAGE */
void
frame_free_page (void *page)
{
  frame_free_fte (frame_find_entry (page));
}
