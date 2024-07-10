#include <stdio.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"


// TS: figure out why 2 faults on every address


int handle_page_fault(PULONG_PTR virtual_address) {

    page_t* free_page;

    PPTE pte = pte_from_va(virtual_address);

    // START LOCK HERE
    EnterCriticalSection(&pte_lock);

    //#### VALID #####
    if (pte->memory.valid == 1) {

        LeaveCriticalSection(&pte_lock);
        return SUCCESS;

    }

    //#### STANDBY/MODIFIED RESCUE ####
    else if (pte->transition.rescuable == 1) {

        // TS: handle when the page is getting written but gets rescued
        // you may have popped from modified list in trimmer, released lock, and now it got rescued
        // however, technically the page is not on the mod list, but physically it has not been removed
        // maybe set page_t flink and blink to null so that unlink returns error

        ULONG64 pfn = pte->transition.frame_number;

        free_page = page_from_pfn(pfn, pfn_base);

        if (free_page->list_type == MODIFIED) {

            EnterCriticalSection(&mod_lock);
            
            if (free_page->list_type == MODIFIED) {

                free_page = list_unlink(&modified_list, pfn);

                if (free_page == NULL) {

                    printf("Could not get page from modified\n");
                    DebugBreak();
                    LeaveCriticalSection(&mod_lock);
                    LeaveCriticalSection(&pte_lock);
                    return ERROR;

                }

            }

            LeaveCriticalSection(&mod_lock);

        }

        if (free_page->list_type == STANDBY) {

            EnterCriticalSection(&standby_lock);

            if (free_page->list_type == STANDBY) {

                // TS: unlink will always work
                free_page = list_unlink(&standby_list, pfn);

                if (free_page == NULL) {

                    printf("Could not get page from standby\n");
                    DebugBreak();
                    LeaveCriticalSection(&standby_lock);
                    LeaveCriticalSection(&pte_lock);
                    return ERROR;

                }

            }
            
            LeaveCriticalSection(&standby_lock);

        }

        if (MapUserPhysicalPages(virtual_address, 1, &pfn) == FALSE) {

            printf("Could not remap modified rescue\n");

            DebugBreak();
                    
            LeaveCriticalSection(&pte_lock);

            return ERROR;

        }
                
        pte->memory.valid = 1;
        // TS: make sure frame number is same bit offset
        pte->transition.rescuable = 0;

        ULONG64 disk_addr = free_page->disk_address;

        LeaveCriticalSection(&pte_lock);

        EnterCriticalSection(&mod_lock);
        pagefile_state[disk_addr] = FREE;
        LeaveCriticalSection(&mod_lock);

        return SUCCESS;

    }

    // #### BRAND NEW #####
    else if (pte->entire_format == 0) {

        // brand new VA; never been accessed before

        free_page = list_pop(&free_list);

        if (free_page == NULL) {
            
            EnterCriticalSection(&standby_lock);
            free_page = list_pop(&standby_list);

            if (free_page == NULL) {

                LeaveCriticalSection(&standby_lock);
                
                LeaveCriticalSection(&pte_lock);

                SetEvent(trim_event);
        
                WaitForSingleObject(fault_event, INFINITE);

                return ERROR;

            }

            // TS: memset to 0 so we don't have residual old contents

            free_page->pte->disk.always_zero2 = 0;
            free_page->pte->disk.disk_address = free_page->disk_address;

            LeaveCriticalSection(&standby_lock);

            // continue to code below to map new VA
        
        }
    }

    //#### ON DISK ####
    else {

        page_t* free_page;

        free_page = list_pop(&free_list);

        if (free_page == NULL) {

            EnterCriticalSection(&standby_lock);
            free_page = list_pop(&standby_list);

            if (free_page == NULL) {

                LeaveCriticalSection(&standby_lock);
                LeaveCriticalSection(&pte_lock);

                SetEvent(trim_event);
        
                WaitForSingleObject(fault_event, INFINITE);

                return ERROR;

            }

            free_page->pte->disk.disk_address = free_page->disk_address;
            free_page->pte->disk.always_zero2 = 0;

            LeaveCriticalSection(&standby_lock);

        }

        // TS: move this to initialization file
        mod_page_va2 = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);

        if (mod_page_va2 == NULL) {

            printf("Could not allocate mod va 2\n");

            DebugBreak();

            return ERROR;

        }

        ULONG64 pfn = pfn_from_page(free_page, pfn_base);

        if (MapUserPhysicalPages(mod_page_va2, 1, &pfn) == FALSE) {

            printf("Could not map mod va 2\n");

            LeaveCriticalSection(&pte_lock);

            DebugBreak();

        }

        void* src = &pagefile_contents[pte->disk.disk_address * PAGE_SIZE];

        memcpy(mod_page_va2, src, PAGE_SIZE);

        if (MapUserPhysicalPages(mod_page_va2, 1, NULL) == FALSE) {

            printf("Could not unmap mod va 2\n");

            LeaveCriticalSection(&pte_lock);

            DebugBreak();

        }

        PTE new_contents;

        ULONG64 disk_addr = pte->disk.disk_address;

        new_contents.entire_format = 0;

        new_contents.memory.valid = 1;
        new_contents.memory.frame_number = pfn;
        
        *pte = new_contents;

        free_page->pte = pte;

        if (MapUserPhysicalPages(virtual_address, 1, &pfn) == FALSE) {

            printf("Could not map disk VA to page\n");

            LeaveCriticalSection(&pte_lock);

            DebugBreak();

        }

        LeaveCriticalSection(&pte_lock);

        EnterCriticalSection(&mod_lock);
        pagefile_state[disk_addr] = FREE;
        LeaveCriticalSection(&mod_lock);

        return SUCCESS;

    }



    ULONG64 pfn = pfn_from_page(free_page, pfn_base);

    if (MapUserPhysicalPages (virtual_address, 1, &pfn) == FALSE) {

        printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", virtual_address, pfn);

        DebugBreak();

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