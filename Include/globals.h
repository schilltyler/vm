#ifndef VM_GLOBALS_H
#define VM_GLOBALS_H

#include "../Include/page.h"
#include "../Include/pageTable.h"
#include "../Include/pagefault.h"

// Global variables
extern PTE* pte_base;
extern PULONG_PTR vmem_base;
extern page_t* pfn_base;
extern listhead_t free_list;
extern ULONG_PTR num_ptes;


// Global definitions
#define PAGE_SIZE                   4096
#define MB(x)                       ((x) * 1024 * 1024)
#define VIRTUAL_ADDRESS_SIZE        MB(16)
#define VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS        (VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR))
#define NUMBER_OF_PHYSICAL_PAGES   ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / 64) // ~1% of virtual address space

#endif