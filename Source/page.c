#include <stdio.h>
#include <windows.h>
#include "../Include/page.h"
#include "../Include/initialize.h"
#include "../Include/debug.h"
#include <assert.h>

/**
 * TS:
 * Explanation for this? (Told myself I might need this later)
 * #define byte_offset(va) ((ULONG_PTR) va & ~(PAGE_SIZE - 1))
 */


page_t* page_create(page_t* pfn_base, ULONG_PTR page_num) {

    page_t* new_page = pfn_base + page_num;
    page_t* base_page = VirtualAlloc(new_page, sizeof(page_t), MEM_COMMIT, PAGE_READWRITE);

    /**
     * TS:
     * Explanation for this?
     */
    C_ASSERT((PAGE_SIZE % sizeof(page_t)) == 0);
    
    if (new_page == NULL) {
        printf("Could not create page\n");
        return NULL;
    }

    new_page->pte = NULL;

    return new_page;
}

void list_insert(listhead_t* listhead, page_t* new_page) {

    /**
     * Inserts at the head
     */

    if (new_page->write_in_progress == 1) {

        printf("Page still marked as rescued or write in progress\n");

        while(TRUE) {


        }

    }

    new_page->flink = (page_t*) listhead->flink;
    listhead->flink->blink = (listhead_t*) new_page;
    listhead->flink = (listhead_t*) new_page;
    new_page->blink = (page_t*) listhead;

    #if DEBUG_PAGE
    CaptureStackBackTrace(0, 8, new_page->backtrace, NULL);
    copy_page_fields(new_page);
    #endif

    #if CIRCULAR_LOG
    log_page(new_page);
    #endif

    listhead->list_size += 1;

    return;
}

void list_insert_tail(listhead_t* listhead, page_t* new_page) {

    new_page->blink = (page_t*) listhead->blink;
    new_page->flink = (page_t*) listhead;
    listhead->blink->flink = (listhead_t*) new_page;
    listhead->blink = (listhead_t*) new_page;

    listhead->list_size += 1;
}

page_t* list_pop(listhead_t* listhead) {

    if (listhead->flink == listhead) {
        return NULL;
    }

    page_t* popped_page = (page_t*) listhead->flink;
    listhead->flink = listhead->flink->flink;
    listhead->flink->blink = listhead;

    #if DEBUG_PAGE
    CaptureStackBackTrace(0, 8, popped_page->backtrace, NULL);
    copy_page_fields(popped_page);
    #endif

    popped_page->flink = NULL;
    popped_page->blink = NULL;

    #if CIRCULAR_LOG
    log_page(popped_page);
    #endif

    listhead->list_size -= 1;

    return popped_page;
}

void list_unlink(listhead_t* listhead, ULONG64 pfn) {

    if (listhead->flink == listhead) {
        DebugBreak();
    }

    page_t* page = page_from_pfn(pfn, g_pfn_base);

    /**
     * TS:
     * Debugger is not working currently so this will stop us
     * at the error and then hang so that we can attach it to
     * the debugger (which does work)
     */
    if (page->flink == NULL && page->blink == NULL) {

        printf("Page flink and blink are NULL\n");

        while(TRUE) {


        }

    }

    // adjust links
    page->flink->blink = page->blink;
    page->blink->flink = page->flink;

    #if DEBUG_PAGE
    CaptureStackBackTrace(0, 8, page->backtrace, NULL);
    copy_page_fields(page);
    #endif

    #if CIRCULAR_LOG
    log_page(page);
    #endif

    page->flink = NULL;
    page->blink = NULL;

    // TS: make list type as active
    listhead->list_size -= 1;

    return;

}