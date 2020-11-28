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
  lock_acquire (&(t->sup_page_table_lock));
  list_push_back (&t->sup_page_table, &(spte->elem));
  lock_release (&(t->sup_page_table_lock));
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
  if (frame == NULL)
    return NULL;
  for (struct list_elem *e = list_begin (&t->sup_page_table);
    e != list_end (&t->sup_page_table); e = list_next (e))
    {
      if (list_entry (e, struct sup_page_table_entry, elem)->fte == NULL)
        continue;
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
  if(spte->status == ON_FRAME && spte->file != NULL)
    sup_page_write_page_mmap_to_filesys (spte, spte->fte->frame);
  if (spte->status == ON_FRAME || spte->status == FROM_FILESYS || 
    spte->status == FROM_FILESYS_SEGMENTS)
    frame_free_fte (spte->fte);
  if(spte->status == IN_SWAP)
    swap_free(spte->swap_index);
  list_remove (&(spte->elem));
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
struct sup_page_table_entry*
sup_page_install_zero_page (void *vaddr)
{
  struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
  if(spte == NULL)
    return NULL;

  spte->user_vaddr = vaddr;
  spte->status = ALL_ZERO;
  spte->owner = thread_current();
  spte->fte = NULL;
  /* Assigned to NULL temporarily 
     But actually how to handle this? */
  spte->dirty = false;
  spte->accessed = false;
  spte->file = NULL;
  spte->file_offset = 0;
  spte->file_bytes = 0;
  spte->zero_bytes = 0;
  spte->writable = true;
  sup_push_to_table(thread_current(),spte);
  return spte;
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
  spte->access_time = timer_ticks();
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

  if (!pagedir_is_dirty (spte->owner->pagedir, spte->user_vaddr))
    return true;

  /* Set the position of write */
  file_seek (spte->file, spte->file_offset);

  /* Read bytes from the file and store in the frame */
  off_t n_write = file_write (spte->file, frame, spte->file_bytes);
  if(n_write != spte->file_bytes)
    return false;
  
  return true;
}

/* Load data as a page from file. 
   Used when loading segments. */
struct sup_page_table_entry *
sup_page_install_mmap_page (struct thread *t, void *uaddr, 
  struct file *f, off_t offset, uint32_t file_bytes, 
  uint32_t zero_bytes, bool writable)
{
  ASSERT (file_bytes + zero_bytes == PGSIZE);
  struct sup_page_table_entry *spte = sup_page_allocate_mmap_page (t, 
    uaddr, f, offset, file_bytes, zero_bytes, writable);
  spte->status = FROM_FILESYS_SEGMENTS;
  return spte;
}

/* Load data as a page from file. 
   Used when mapping file to memory. */
struct sup_page_table_entry *
sup_page_allocate_mmap_page (struct thread *t, void *uaddr, 
  struct file *f, off_t offset, uint32_t file_bytes, 
  uint32_t zero_bytes, bool writable)
{
  ASSERT (file_bytes + zero_bytes == PGSIZE);

  struct sup_page_table_entry *spte = malloc (
    sizeof (struct sup_page_table_entry));

  spte->owner = t;
  spte->user_vaddr = uaddr;
  spte->fte = NULL;
  spte->status = FROM_FILESYS;
  spte->dirty = false;
  spte->file = f;
  spte->file_offset = offset;
  spte->file_bytes = file_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->access_time = timer_ticks ();
  sup_push_to_table (t, spte);
  return spte;
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
  if (vaddr == NULL)
    return false;
  
  struct sup_page_table_entry* spte = sup_page_find_entry_uaddr (curr, vaddr);
  if(spte == NULL) {
    return false;
  }
  if(spte->status == ON_FRAME) {
    // already loaded
    return true;
  }
  uint32_t *pagedir = curr->pagedir;
  bool writable = true;

  /* Allocate the frame if no corresponding frame */
  if (spte->fte == NULL) 
  {
    spte->fte = frame_allocate_page (spte, PAL_USER|PAL_ZERO);
  }
  void* frame = spte->fte->frame;
  switch(spte->status)
  {
    case ALL_ZERO:
      memset(frame, 0, PGSIZE);
      spte->access_time = timer_ticks();
      break;
    
    case ON_FRAME:
      break;
    
    case IN_SWAP:
      read_from_swap(spte->swap_index,frame);
      spte->access_time = timer_ticks();
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
    
    case FROM_FILESYS_SEGMENTS:
      /* Load the content of corresponding file */
      if (!sup_page_load_page_mmap_from_filesys (spte, frame))
        {
          /* Free the page if not succeess */
          sup_page_free_spte (spte);
          return false;
        }
      writable = spte->writable;

      /* Clear the file info so they can be stroed in swap if evicted */
      spte->file = NULL;
      spte->file_bytes = 0;
      spte->file_offset = 0;
      break;
  }
  pagedir_set_page(pagedir,vaddr,frame,writable);
  spte->status = ON_FRAME;
  return true; /* Do nothing */
}
