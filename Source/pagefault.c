#include <stdio.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"

// TS: Not able to map everything

#define SUCCESS 1
#define ERROR 0

CRITICAL_SECTION pte_lock;


// Queue this to start back up again after trimming is done
int handle_page_fault(PULONG_PTR virtual_address) {

    //WaitForSingleObject();

    page_t* free_page;

    PPTE pte = pte_from_va(virtual_address);

    // START LOCK HERE
    EnterCriticalSection(&pte_lock);

    if (pte->memory.valid == 1) {

        LeaveCriticalSection(&pte_lock);
        return SUCCESS;

    }

    // TS: fix in case 0 is a valid frame num
    else if (pte->transition.frame_number != 0) {

        // Rescue modified page (standby right now)
        pte->memory.valid = 1;

        LeaveCriticalSection(&pte_lock);
        return SUCCESS;

    }

    else if (pte->disk.on_disc == 1) {

        // Get new frame for disk contents
        free_page = list_pop(&free_list);

        if (free_page == NULL) {

            free_page = list_pop(&standby_list);

            if (free_page == NULL) {

                LeaveCriticalSection(&pte_lock);

                WaitForSingleObject(fault_event, INFINITE);

                return ERROR;

            }

        }

        // TS: deal with getting contents from memory

    }

    else {

        free_page = list_pop(&free_list);

        if (free_page == NULL) {

            free_page = list_pop(&standby_list);

            if (free_page == NULL) {
                
                LeaveCriticalSection(&pte_lock);
        
                WaitForSingleObject(fault_event, INFINITE);

                return ERROR;

            }

            // get old VA
            PULONG_PTR old_addr = va_from_pte(free_page->pte);

            // unmap this PA from old VA
            if (MapUserPhysicalPages (old_addr, 1, NULL) == FALSE) {

                printf ("full_virtual_memory_test : could not unmap trim_va %p\n", old_addr);

                LeaveCriticalSection(&pte_lock);

                return ERROR;
            }

            free_page->pte->transition.frame_number = 0;

            // continue to code below to map new VA
        
        }
    }



    ULONG64 pfn = pfn_from_page(free_page, pfn_base);

    if (MapUserPhysicalPages (virtual_address, 1, &pfn) == FALSE) {

        printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", virtual_address, pfn);

        LeaveCriticalSection(&pte_lock);

        return ERROR;
    }

    pte->memory.valid = 1;
    pte->memory.frame_number = pfn;

    // need this for when I trim page from something like standby and want to cut off old pte to replace with new one
    free_page->pte = pte;

    //END LOCK HERE
    LeaveCriticalSection(&pte_lock);

    
    if (free_list.list_size + standby_list.list_size < physical_page_count / 4) {
        SetEvent(trim_event);
    }
    

    return SUCCESS;
}