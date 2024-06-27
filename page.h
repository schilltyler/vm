#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdio.h>
#include <windows.h>
#include "./pageTable.h"

#define PAGE_SIZE                   4096
#define MB(x)                       ((x) * 1024 * 1024)
#define VIRTUAL_ADDRESS_SIZE        MB(16)
#define VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS        (VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR))
#define NUMBER_OF_PHYSICAL_PAGES   ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / 64) // ~1% of virtual address space

// this is a PFN *Entry*
typedef struct page {

    // each page needs to be linked to each other
    struct page* flink;
    struct page* blink;

    // TS: fix this later
    ULONG64 structure_pad;

    // this page (that has the physical page address) is connected to a va through this
    PTE* pte;

} page_t;

typedef struct listhead {
    struct listhead* flink;
    struct listhead* blink;
} listhead_t;

// get page from pfn
extern page_t* page_from_pfn(ULONG64 pfn, page_t* pfn_base);

// Create a page node
page_t* page_create(page_t* pfn_base, ULONG_PTR page_num);

// Insert a page into the list
void list_insert(listhead_t* listhead, page_t* new_page);

// Take a page from the list
page_t* list_pop(listhead_t* listhead);

#endif