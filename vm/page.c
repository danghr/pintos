#include "vm/page.h"
#include <stdlib.h>
#include <string.h>
#include <debug.h>
#include "devices/timer.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "filesys/off_t.h"

/* Push the page table SPTE to the sup_page_table in current thread */
static void
sup_push_to_table (struct thread *t, struct sup_page_table_entry *spte)
{
  ASSERT (!in_list (&(spte->elem), &t->sup_page_table));

  list_push_back (&t->sup_page_table, &(spte->elem));
}

/* Find the corresponding page table entry in thread T according to the 
   given user virtual address. 
   Return NULL if not found. */
struct sup_page_table_entry *
sup_page_find_entry_uaddr (struct thread *t, void *user_vaddr)
{
  for (struct list_elem *e = list_begin (&t->sup_page_table);
       e != list_end (&t->sup_page_table); e = list_next (e))
    {
      if (list_entry (e, struct sup_page_table_entry, elem)->user_vaddr
          == user_vaddr)
        return list_entry (e, struct sup_page_table_entry, elem);
    }
  return NULL;
}

/* Find the corresponding page table entry in thread T according to 
   the given frame address. 
   Return NULL if not found. */
struct sup_page_table_entry *
sup_page_find_entry_frame (struct thread *t, void *frame)
{
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
struct sup_page_table_entry *
sup_page_allocate_page (enum palloc_flags flags)
{
  /* This function should ONLY be used to allocate frames from user pool 
     Refer to 4.1.5 (Really? ) */
  ASSERT (flags == PAL_USER || 
          flags == (PAL_ZERO | PAL_USER) ||
          flags == (PAL_ASSERT | PAL_USER) ||
          flags == (PAL_ZERO | PAL_ASSERT | PAL_USER));
  
  struct thread *t = thread_current ();

  /* Allocate memory for SPTE */
  struct sup_page_table_entry *spte = 
    malloc (sizeof (struct sup_page_table_entry));
  if (spte == NULL)
    return NULL;

  /* Try to allocate a page */
  struct frame_table_entry *fte = frame_allocate_page (spte, flags);

  if (fte == NULL)
    {
      free (spte);
      return NULL;
    }

  spte->owner = t;
  spte->fte = fte;
  fte->spte = spte;
  /* Assigned to NULL temporarily 
     But actually how to handle this? */
  spte->user_vaddr = NULL; 
  spte->dirty = false;
  spte->accessed = false;
  spte->file = NULL;
  spte->file_offset = 0;
  spte->file_bytes = 0;
  spte->zero_bytes = 0;
  spte->writable = true;
  spte->access_time = timer_ticks ();
  if (flags == (PAL_ZERO | PAL_USER) ||
      flags == (PAL_ZERO | PAL_ASSERT | PAL_USER))
    {
      spte->status = ALL_ZERO;  /* QUESTION: What should others be? */
    }
  sup_push_to_table (t, spte);
  return spte;
  
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
  sup_page_free_spte (sup_page_find_entry_uaddr (thread_current (), uaddr));
}

/* Free the given page table entry in current thread according to 
   the given frame address */
void
sup_page_free_page_frame (void *frame)
{
  sup_page_free_spte (sup_page_find_entry_frame (thread_current (), frame));
}

/* Install a new page with all zeros, used in page fault handler
   Return true if success, or false otherwise */
bool 
sup_page_install_zero_page (void *vaddr)
{
  struct sup_page_table_entry *spte = sup_page_allocate_page ((PAL_ZERO | PAL_USER));
  if(spte == NULL)
    return false;

  spte->user_vaddr = vaddr;
  spte->status = ALL_ZERO;
  return true;
}

/* Reload the page of the memory mapped file and read the data of the file 
   to the given frame. 
   Returns true if succeeds, false otherwise. */
bool 
sup_page_load_page_mmap_from_filesys (struct sup_page_table_entry *spte, 
  void *frame)
{
  ASSERT (spte->file_bytes + spte->zero_bytes == PGSIZE);

  /* Set the position of read */
  file_seek (spte->file, spte->file_offset);

  /* Read bytes from the file and store in the frame */
  off_t n_read = file_read (spte->file, frame, spte->file_bytes);
  if(n_read != spte->file_bytes)
    return false;

  /* Remaining bytes are still zero
     Write them in the frame */
  memset (frame + n_read, 0, spte->zero_bytes);
  return true;
}

/* Write the data in page of the memory mapped file to the corresponding 
   file.
   Returns true if succeeds, false otherwise. */
bool 
sup_page_write_page_mmap_to_filesys (struct sup_page_table_entry *spte, 
  void *frame)
{
  ASSERT (spte->file_bytes + spte->zero_bytes == PGSIZE);

  /* Set the position of write */
  file_seek (spte->file, spte->file_offset);

  /* Read bytes from the file and store in the frame */
  off_t n_write = file_write (spte->file, frame, spte->file_bytes);
  if(n_write != spte->file_bytes)
    return false;
  
  return true;
}

/* Insert a new page for memory mapped file with the given information */
void 
sup_page_install_mmap_page (struct thread *t UNUSED, void *uaddr, 
  struct file *f, off_t offset, uint32_t file_bytes, 
  uint32_t zero_bytes, bool writable)
{
  struct sup_page_table_entry *spte = sup_page_allocate_page ((PAL_USER | PAL_ZERO));
  if (spte == NULL)
    return ;

  spte->user_vaddr = uaddr;
  spte->status = FROM_FILESYS;
  spte->dirty = false;
  spte->file = f;
  spte->file_offset = offset;
  spte->file_bytes = file_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  sup_page_load_page_mmap_from_filesys (spte, spte->fte->frame);
}

/* Remove the given page for memory mapped file with the given information */
void 
sup_page_remove_mmap_page (struct thread *t, void *uaddr)
{
  struct sup_page_table_entry *spte = sup_page_find_entry_uaddr (t, uaddr);
  if (spte == NULL)
    return ;
  sup_page_write_page_mmap_to_filesys (spte, spte->fte->frame);
  sup_page_free_spte (spte);
}

bool
load_page (struct thread *curr, void* vaddr)
{
  struct sup_page_table_entry* spte = sup_page_find_entry_uaddr(curr, vaddr);
  if(spte == NULL) {
    return false;
  }
  if(spte->status == ON_FRAME) {
    // already loaded
    return true;
  }
  uint32_t *pagedir = curr->pagedir;
  bool writable = true;
  void* frame = spte->fte->frame;

  switch(spte->status)
  {
    case ALL_ZERO:
      memset(frame, 0, PGSIZE);
      break;
    
    case ON_FRAME:
      break;
    
    case IN_SWAP:
      read_from_swap(spte->swap_index,frame);
      break;
    
    case FROM_FILESYS:
      /* Load the content of corresponding file */
      if (!sup_page_load_page_mmap_from_filesys (spte, frame))
        {
          /* Free the page if not succeess */
          sup_page_free_spte (spte);
          return false;
        }
      writable = spte->writable;
      break;
  }
  pagedir_set_page(pagedir,vaddr,frame,writable);
  spte->status = ON_FRAME;
  return true; /* Do nothing */
}
