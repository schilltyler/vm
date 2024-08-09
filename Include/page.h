#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdio.h>
#include <windows.h>
#include "../Include/pagetable.h"
#include "../Include/debug.h"


typedef struct listhead {

    struct listhead* flink;
    struct listhead* blink;
    ULONG64 list_size;

} listhead_t;


typedef struct page {

    struct page* flink;
    struct page* blink;

    ULONG64 disk_address;

    PTE* pte;

    ULONG64 list_type:2;

    ULONG64 write_in_progress:1;

    ULONG64 was_rescued:1;

    ULONG64 padding[3];

    #if DEBUG_PAGE
    PVOID backtrace[8];
    #endif

} page_t;


extern page_t* page_from_pfn(ULONG64 pfn, page_t* pfn_base);


page_t* page_create(page_t* pfn_base, ULONG_PTR page_num);


void list_insert(listhead_t* listhead, page_t* new_page);


page_t* list_pop(listhead_t* listhead);


void list_unlink(listhead_t* listhead, ULONG64 pfn);

#endif