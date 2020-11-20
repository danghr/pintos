#include "vm/page.h"
#include <stdlib.h>
#include <debug.h>
#include "devices/timer.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

/* Push the page table SPTE to the sup_page_table in the givne thread T */
static void
sup_push_to_table (struct thread *t, struct sup_page_table_entry *spte)
{
  ASSERT (!in_list (&(spte->elem), &t->sup_page_table));

  list_push_back (&t->sup_page_table, &(spte->elem));
}

/* Find the corresponding page table entry in thread T according to the 
   given VADDR. 
   Return NULL if not found. */
struct sup_page_table_entry *
sup_page_find_entry (struct thread *t, void *vaddr)
{
  for (struct list_elem *e = list_begin (&t->sup_page_table);
       e != list_end (&t->sup_page_table); e = list_next (e))
    {
      if (list_entry (e, struct sup_page_table_entry, elem)->page
          == vaddr)
        return list_entry (e, struct sup_page_table_entry, elem);
    }
  return NULL;
}

/* Allocate a page table according to given FLAGS in thread T. 
   Return the address of the allocated page, or NULL if fails. */
struct sup_page_table_entry *
sup_page_allocate_page (struct thread *t, enum palloc_flags flags)
{
  /* This function should ONLY be used to allocate frames from user pool 
     Refer to 4.1.5 (Really? ) */
  ASSERT (flags == PAL_USER || 
          flags == (PAL_ZERO | PAL_USER) ||
          flags == (PAL_ASSERT | PAL_USER) ||
          flags == (PAL_ZERO | PAL_ASSERT | PAL_USER));
  
  /* Try to allocate a page */
  void *page = palloc_get_page (flags);

  /* If allocate success */
  if (page != NULL)
    {
      /* Push into the list */
      struct sup_page_table_entry *spte = 
        malloc (sizeof (struct sup_page_table_entry));
      spte->page = page;
      spte->owner = t;
      spte->dirty = false;      /* Should it? */
      spte->accessed = false;   /* Should it? */
      spte->access_time = timer_ticks ();
      p_push_to_table (t, spte);
      return spte;
    }
  /* Need to implement evicting a frame according to LRU from 
     the current memory and reallocate the frame. */
  return NULL;
}

/* Free the given page table entry SPTE and the corresponding 
   page table. */
void sup_page_free_spte (struct sup_page_table_entry *spte)
{
  list_remove (&(spte->elem));

  palloc_free_page (spte->page);
  free (spte);
}

/* Free the given page table entry in thread T according to 
   the given PAGE */
void sup_page_free_page (struct thread *t, void *vaddr)
{
  sup_page_free_spte (sup_page_find_entry (t, vaddr));
}

bool 
sup_install_zero_page(struct thread *curr_thread,void *vaddr)
{
  struct sup_page_table_entry *spte;
  spte = (struct sup_page_table_entry *) malloc(sizeof(struct sup_page_table_entry);
  if(spte == NULL)
    return false;
  spte->page = vaddr;
  spte->owner = curr_thread;
  spte->dirty = false;
  spte->accessed = false;
  /* Set the time 1 for debug*/
  spte->access_time = 1; 
  spte->status = ALL_ZERO;
  sup_push_to_table(curr_thread,spte);
  return true;
}
bool
load_page(struct thread *t,)
