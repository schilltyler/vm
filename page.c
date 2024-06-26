#include <stdio.h>
#include <windows.h>
#include "page.h"
//#include "./globals.h"


page_t* page_create(page_t* pfn_base, ULONG_PTR page_num) {

    page_t* new_page = VirtualAlloc(pfn_base + page_num, sizeof(page_t), MEM_COMMIT, PAGE_READWRITE);
    
    if (new_page == NULL) {
        printf("Could not create page\n");
        return NULL;
    }

    // set pfn
    new_page->pfn = page_num;
    new_page->pte = NULL;

    return new_page;
}

// insert at head
void list_insert(listhead_t* listhead, page_t* new_page) {

    // set links
    new_page->flink = (page_t*) listhead->flink;
    listhead->flink->blink = (listhead_t*) new_page;
    listhead->flink = (listhead_t*) new_page;
    new_page->blink = (page_t*) listhead;

    return;
}

page_t* list_pop(listhead_t* listhead) {
    // check if list is empty
    if (listhead->flink == listhead) {
        printf("Nothing to pop, the list is empty\n");
        return NULL;
    }

    // adjust links
    page_t* popped_page = (page_t*) listhead->flink;
    listhead->flink = listhead->flink->flink;
    listhead->flink->blink = listhead;

    return popped_page;
}