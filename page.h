#include <stdio.h>
#include <windows.h>
#include "./pageTable.h"

// this is a PFN *Entry*
typedef struct page {

    // each page needs to be linked to each other
    struct page* flink;
    struct page* blink;

    // each page has a physical frame number that corresponds
    ULONG64 pfn;

    // this page (that has the physical page address) is connected to a va through this
    PTE* pte;

} page_t;

typedef struct listhead {
    struct listhead* flink;
    struct listhead* blink;
} listhead_t;

// Create a page node
page_t* page_create(ULONG_PTR page_num);

// Create list with the first item
//page_t* list_create(ULONG_PTR page_num);

// Insert a page into the list
void list_insert(listhead_t* listhead, page_t* new_page);

// Take a page from the list
page_t* list_pop(listhead_t* listhead);

