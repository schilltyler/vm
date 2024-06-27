#include <stdio.h>
#include "../Include/page.h"
#include "../Include/pageTable.h"
#include "../Include/globals.h"

#define SUCCESS 1
#define ERROR 0


int handle_page_fault(PULONG_PTR virtual_address) {

    page_t* free_page = list_pop(&free_list);

    // free list does not have any pages left
    // so . . . trim random active page
    if (free_page == NULL) {

        // start a trimming thread
        #if 0
        HANDLE threads[1];
        PARAM_STRUCT params;
        params.test_type = test;
        params.state = 0;
        threads[0] = CreateThread(NULL, 0, trim_thread, &params, 0, NULL);
        WaitForSingleObject(threads[0], INFINITE);
        CloseHandle(threads[0]);
        #endif
        
        for (PPTE trim_pte = pte_base; trim_pte < pte_base + num_ptes; trim_pte ++) {
            
            // found our page, let's trim
            if (trim_pte->memory.valid == 1) {

                page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                list_insert(&free_list, curr_page);

                PULONG_PTR trim_va = va_from_pte(trim_pte);

                // unmap the va from the pa
                if (MapUserPhysicalPages (trim_va, 1, NULL) == FALSE) {

                    printf ("full_virtual_memory_test : could not unmap trim_va %p\n", trim_va);

                    return ERROR;
                }

                // set valid bit to 0
                trim_pte->memory.valid = 0;
                trim_pte->memory.frame_number = 0;

                free_page = list_pop(&free_list);

                if (free_page == NULL) {
                    printf("Could not pop from free_list\n");
                }

                break;

            }

        }    

    }

    ULONG64 pfn = pfn_from_page(free_page, pfn_base);

    if (MapUserPhysicalPages (virtual_address, 1, &pfn) == FALSE) {

        printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", virtual_address, pfn);

        return ERROR;
    }

    PPTE pte = pte_from_va(virtual_address);

    pte->memory.valid = 1;
    pte->memory.frame_number = pfn;

    // need this for when I trim page from something like standby and want to cut off old pte to replace with new one
    free_page->pte = pte;

    // No exception handler needed now since we have connected
    // the virtual address above to one of our physical pages
    // so no subsequent fault can occur.
    *virtual_address = (ULONG_PTR) virtual_address;

    return SUCCESS;
}