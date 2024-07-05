#include <stdio.h>
#include <windows.h>
#include "../Include/page.h"
#include "../Include/initialize.h"

// May need this later
//#define byte_offset(va) ((ULONG_PTR) va & ~(PAGE_SIZE - 1))


page_t* page_create(page_t* pfn_base, ULONG_PTR page_num) {

    page_t* new_page = pfn_base + page_num;
    page_t* base_page = VirtualAlloc(new_page, sizeof(page_t), MEM_COMMIT, PAGE_READWRITE);
    // fix this when fixing structure_padding, commit both halves (base_page and the next page)

    C_ASSERT((PAGE_SIZE % sizeof(page_t)) == 0);
    
    if (new_page == NULL) {
        printf("Could not create page\n");
        return NULL;
    }

    // set pte
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

    // increment list size
    listhead->list_size += 1;

    return;
}

void list_insert_tail(listhead_t* listhead, page_t* new_page) {

    // set links
    new_page->blink = (page_t*) listhead->blink;
    new_page->flink = (page_t*) listhead;
    listhead->blink->flink = (listhead_t*) new_page;
    listhead->blink = (listhead_t*) new_page;

    listhead->list_size += 1;
}

page_t* list_pop(listhead_t* listhead) {

    // check if list is empty
    if (listhead->flink == listhead) {
        return NULL;
    }

    // adjust links
    page_t* popped_page = (page_t*) listhead->flink;
    listhead->flink = listhead->flink->flink;
    listhead->flink->blink = listhead;

    // decrement list size
    listhead->list_size -= 1;

    return popped_page;
}

page_t* list_unlink(listhead_t* listhead, ULONG64 pfn) {

    if (listhead->flink == listhead) {
        return NULL;
    }

    page_t* page = page_from_pfn(pfn, pfn_base);

    // adjust links
    page->flink->blink = page->blink;
    page->blink->flink = page->flink;

    return page;

}