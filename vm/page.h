#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"

/* Entries of supplemental page table */
struct sup_page_table_entry
{
  void *page;               /* User virtual address of the page */
  uint64_t access_time;     /* Record the last access time for LRU */
  bool dirty;
  bool accessed;
  struct thread *owner;     /* Owner thread/process of the page */
  struct list_elem elem;
  enum state status;
};
enum state{
  ALL_ZERO = 0,
  ON_FRAME = 1,
  IN_SWAP = 2
};
struct sup_page_table_entry *sup_page_find_entry (struct thread *, void *);

struct sup_page_table_entry *sup_page_allocate_page 
  (struct thread *, enum palloc_flags);
void sup_page_free_spte (struct sup_page_table_entry *);
void sup_page_free_page (struct thread *, void *);
bool sup_install_zero_page(struct thread *curr_thread,void *vaddr);

#endif /* vm/page.h */
