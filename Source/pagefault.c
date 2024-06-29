#include <stdio.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"

#define SUCCESS 1
#define ERROR 0

CRITICAL_SECTION pte_lock;

// Queue this to start back up again after trimming is done
int handle_page_fault(PULONG_PTR virtual_address) {

    PPTE pte = pte_from_va(virtual_address);

    // START LOCK HERE
    EnterCriticalSection(&pte_lock);
    if (pte->memory.valid == 1) {
        LeaveCriticalSection(&pte_lock);
        return SUCCESS;
    }
    
    #if 0 
    if pte = 0
        do below
    else
        if pte says page is on standby or modified list
            get specific page from that list (pte tells us which one)
            set pte valid bit & page number
            done
        else
            get page from free/standby
            read contents from disk address in pte
            write contents into page from free/stanby (page from 31)
            set pte valid bit/ page num
            done
    #endif

    page_t* free_page = list_pop(&free_list);

    // free list does not have any pages left
    // so . . . trim random active page
    #if 0
    if (free_page == NULL) {

        free_page = list_pop(&standby_list);

        if can't get from standby
            release locks, then wait here until free or standby page appears (set other event)
            retry everything (return to caller)
        else
            we're good, do all of the pte stuff below as if it was a free page

    }
    #endif

    // convert from 

    ULONG64 pfn = pfn_from_page(free_page, pfn_base);

    if (MapUserPhysicalPages (virtual_address, 1, &pfn) == FALSE) {

        printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", virtual_address, pfn);

        return ERROR;
    }

    

    pte->memory.valid = 1;
    pte->memory.frame_number = pfn;

    // need this for when I trim page from something like standby and want to cut off old pte to replace with new one
    free_page->pte = pte;

    //END LOCK HERE
    LeaveCriticalSection(&pte_lock);

    // if getting this page put us below 10% pages left on combined free and standby list (might want to set trim event)
    // below 20% might want to age (talked about dynamic algorithm)
    if (free_list.list_size < physical_page_count * 0.15) {
        SetEvent(trim_event);
    }

    return SUCCESS;
}