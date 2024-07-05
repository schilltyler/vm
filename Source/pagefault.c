#include <stdio.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"


#define SUCCESS 1
#define ERROR 0
#define FREE 0

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

    else if (pte->transition.rescuable == 1) {

        // Rescue modified page (standby right now)

        ULONG64 pfn = pte->transition.frame_number;

        free_page = list_unlink(&standby_list, pfn);

        if (free_page == NULL) {

            printf("Could not get page from standby\n");
            DebugBreak();
            LeaveCriticalSection(&pte_lock);
            return ERROR;

        }

        if (MapUserPhysicalPages(virtual_address, 1, &pfn) == FALSE) {

            printf("Could not remap standby rescue\n");

            DebugBreak();
                    
            LeaveCriticalSection(&pte_lock);

            return ERROR;

        }
                
        pte->memory.valid = 1;
        pte->transition.rescuable = 0;

        LeaveCriticalSection(&pte_lock);
        return SUCCESS;

    }

    else if (pte->disk.on_disc == 0 && pte->disk.accessed == 1) {

        DebugBreak();

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

        // TS: doing the opposite of the mod writer
        // TS: need to use temp va because VA is not mapped
        mod_page_va2 = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);

        if (mod_page_va2 == NULL) {

            printf("Could not allocate mod page va2\n");

            LeaveCriticalSection(&pte_lock);

            return ERROR;

        }

        memcpy(mod_page_va2, &pagefile_contents[pte->disk.disk_address * PAGE_SIZE], PAGE_SIZE);

        pagefile_state[pte->disk.disk_address] = FREE;

        // continue to below and map VA to new PA
    }

    else {

        // brand new VA; never been accessed before

        free_page = list_pop(&free_list);

        if (free_page == NULL) {

            free_page = list_pop(&standby_list);

            if (free_page == NULL) {
                
                LeaveCriticalSection(&pte_lock);

                SetEvent(trim_event);
        
                WaitForSingleObject(fault_event, INFINITE);

                return ERROR;

            }

            free_page->pte->transition.rescuable = 0;
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