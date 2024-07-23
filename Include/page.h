#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdio.h>
#include <windows.h>
#include "../Include/pagetable.h"


typedef struct listhead {
    struct listhead* flink;
    struct listhead* blink;
    // increment in list insert and pop
    ULONG64 list_size;
} listhead_t;


// this is a PFN *Entry*
typedef struct page {

    // each page needs to be linked to each other
    struct page* flink;
    struct page* blink;

    // TS: fix this later
    ULONG64 disk_address;

    // this page (that has the physical page address) is connected to a va through this
    PTE* pte;

    ULONG64 list_type:2;

    ULONG64 padding[3];

    /**
     * TS: don't want to keep this longterm
     * just for debugging
     * Turn it on/off for debugging
     */
    PVOID backtrace[8];

} page_t;

// get page from pfn
extern page_t* page_from_pfn(ULONG64 pfn, page_t* pfn_base);

// Create a page node
page_t* page_create(page_t* pfn_base, ULONG_PTR page_num);

// Insert a page into the list
void list_insert(listhead_t* listhead, page_t* new_page);

// Take a page from the list
page_t* list_pop(listhead_t* listhead);

// Take a page from anywhere in the list
void list_unlink(listhead_t* listhead, ULONG64 pfn);

#endif