#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init(void);
size_t store_in_swap(void *page);
void read_from_swap(size_t index,void *page);
void swap_free(size_t index);
#endif /* vm/swap.h */