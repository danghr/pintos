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
  ALL_ZERO = 0,
  ON_FRAME = 1,
  IN_SWAP = 2,
  FROM_FILESYS = 3
};

/* Entries of supplemental page table */
struct sup_page_table_entry
{
  struct frame_table_entry *fte;   /* Corresponding frame entry */
  
  /* How to handle user virtual address in later operations? 
     Should we handle it during lasy load? */
  void *user_vaddr;         /* User virtual address of the page */
  bool writable;
  enum state status;

  uint64_t access_time;     /* Record the last access time for LRU */
  bool dirty;
  bool accessed;

  /* For swap */
  size_t swap_index;

  /* For memory mapped files */
  struct file *file;
  off_t file_offset;
  off_t file_bytes;
  off_t zero_bytes;

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

bool sup_page_install_zero_page (void *);
void sup_page_install_mmap_page (struct thread *, void *, struct file *, 
  off_t, uint32_t, uint32_t, bool);
bool load_page (struct thread *, void *);

#endif /* vm/page.h */
