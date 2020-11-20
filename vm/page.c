#include "vm/page.h"
#include <stdlib.h>
#include <debug.h>
#include "devices/timer.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

/* Push the page table SPTE to the sup_page_table in current thread */
static void
sup_push_to_table (struct sup_page_table_entry *spte)
{
  struct thread *t = thread_current ();
  ASSERT (!in_list (&(spte->elem), &t->sup_page_table));

  list_push_back (&t->sup_page_table, &(spte->elem));
}

/* Find the corresponding page table entry in current thread according to the 
   given user virtual address. 
   Return NULL if not found. */
struct sup_page_table_entry *
sup_page_find_entry_uaddr (void *user_vaddr)
{
  struct thread *t = thread_current ();
  for (struct list_elem *e = list_begin (&t->sup_page_table);
       e != list_end (&t->sup_page_table); e = list_next (e))
    {
      if (list_entry (e, struct sup_page_table_entry, elem)->fte->frame
          == user_vaddr)
        return list_entry (e, struct sup_page_table_entry, elem);
    }
  return NULL;
}

/* Find the corresponding page table entry in current thread according to 
   the given frame address. 
   Return NULL if not found. */
struct sup_page_table_entry *
sup_page_find_entry_frame (void *frame)
{
  struct thread *t = thread_current ();
  for (struct list_elem *e = list_begin (&t->sup_page_table);
       e != list_end (&t->sup_page_table); e = list_next (e))
    {
      if (list_entry (e, struct sup_page_table_entry, elem)->fte->frame
          == frame)
        return list_entry (e, struct sup_page_table_entry, elem);
    }
  return NULL;
}

/* Allocate a page table according to given FLAGS in current thread. 
   Return the address of the allocated page, or NULL if fails. */
void *
sup_page_allocate_page (enum palloc_flags flags)
{
  /* This function should ONLY be used to allocate frames from user pool 
     Refer to 4.1.5 (Really? ) */
  ASSERT (flags == PAL_USER || 
          flags == (PAL_ZERO | PAL_USER) ||
          flags == (PAL_ASSERT | PAL_USER) ||
          flags == (PAL_ZERO | PAL_ASSERT | PAL_USER));
  
  struct thread *t = thread_current ();

  /* Try to allocate a page */
  struct frame_table_entry *fte = frame_allocate_page (t, flags);

  /* If allocate success */
  if (fte != NULL)
    {
      /* Push into the list */
      struct sup_page_table_entry *spte = 
        malloc (sizeof (struct sup_page_table_entry));
      if (spte == NULL)
        {
          frame_free_page (fte);
          return NULL;
        }
      spte->fte = fte;
      /* Assigned to NULL temporarily 
         But actually how to handle this? */
      spte->user_vaddr = NULL; 
      spte->dirty = false;
      spte->accessed = false;
      spte->access_time = timer_ticks ();
      if (flags == (PAL_ZERO | PAL_USER) ||
          flags == (PAL_ZERO | PAL_ASSERT | PAL_USER))
        {
          spte->status = ALL_ZERO;
        }
      sup_push_to_table (spte);
      return spte->fte->frame;
    }
  
  /* Eviction is handled in frame allocation, so if that returns NULL,
     the frame allocation process fails, and we just return NULL */
  return NULL;
}

/* Free the given page table entry SPTE and the corresponding 
   page table. */
void sup_page_free_spte (struct sup_page_table_entry *spte)
{
  list_remove (&(spte->elem));

  frame_free_fte (spte->fte);
  free (spte);
}

/* Free the given page table entry in current thread according to 
   the given user virtual address */
void
sup_page_free_page_uaddr (void *uaddr)
{
  sup_page_free_spte (sup_page_find_entry_uaddr (uaddr));
}

/* Free the given page table entry in current thread according to 
   the given frame address */
void
sup_page_free_page_frame (void *frame)
{
  sup_page_free_spte (sup_page_find_entry_frame (frame));
}

/* Install a new page with all zeros, used in page fault handler
   Return true if success, or false otherwise */
bool 
sup_page_install_zero_page (void *vaddr)
{
  struct sup_page_table_entry *spte = 
    sup_page_allocate_page (PAL_USER | PAL_ZERO);
  if(spte == NULL)
    return false;
  spte->user_vaddr = vaddr;
  /* Manually set the time 1 for debug */
  spte->access_time = 1; 
  return true;
}

bool
load_page (void *vaddr UNUSED)
{
  return false; /* Do nothing */
}
