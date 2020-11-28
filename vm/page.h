#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/frame.h"
#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/file.h"

/* States of the supplemental page table entry */
enum state
{
  ALL_ZERO = 0,                   /* Zero page */
  ON_FRAME = 1,                   /* Now on frame */
  IN_SWAP = 2,                    /* Now in swap */
  FROM_FILESYS = 3,               /* Loaded from file, evict to file */
  FROM_FILESYS_SEGMENTS = 4       /* Loaded from file, evict to swap */
};

/* Entries of supplemental page table */
struct sup_page_table_entry
{
  struct frame_table_entry *fte;  /* Corresponding FTE */
  
  struct thread *owner;           /* Owner thread */
  void *user_vaddr;               /* User virtual address */
  bool writable;                  /* Writable flag */
  enum state status;              /* Current status of the page */

  uint64_t access_time;           /* Last access time */
  bool dirty;                     /* Dirty flag */
  bool accessed;                  /* Accessed flag */

  /* For swap */
  size_t swap_index;              /* Swap index if swapped out */

  /* For memory mapped files */
  struct file *file;              /* Mapped file */
  off_t file_offset;              /* Offset in the file for this page */
  off_t file_bytes;               /* Bytes from data */
  off_t zero_bytes;               /* Bytes of extra zeros */

  /* List element */
  struct list_elem elem;
};

struct sup_page_table_entry *sup_page_find_entry_uaddr
  (struct thread *, void *);
struct sup_page_table_entry *sup_page_find_entry_frame
  (struct thread *, void *);

struct sup_page_table_entry *sup_page_allocate_page (enum palloc_flags);
void sup_page_free_spte (struct sup_page_table_entry *);
void sup_page_free_page_uaddr (void *);
void sup_page_free_page_frame (void *);

struct sup_page_table_entry* sup_page_install_zero_page (void *);
bool sup_page_load_page_mmap_from_filesys (struct sup_page_table_entry *, 
  void *);
bool sup_page_write_page_mmap_to_filesys (struct sup_page_table_entry *, 
  void *);
struct sup_page_table_entry*
sup_page_install_mmap_page (struct thread *t , void *uaddr, 
  struct file *f, off_t offset, uint32_t file_bytes, 
  uint32_t zero_bytes, bool writable);
void sup_page_remove_mmap_page (struct thread *, void *);
void sup_page_mmap_write_back (struct sup_page_table_entry *);
bool load_page (struct thread *, void *);
struct sup_page_table_entry *
sup_page_allocate_mmap_page (struct thread *t UNUSED, void *uaddr, 
  struct file *f, off_t offset, uint32_t file_bytes, 
  uint32_t zero_bytes, bool writable);
#endif /* vm/page.h */
