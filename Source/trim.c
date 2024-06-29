#include <stdio.h>
#include <Windows.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"


void trim_thread(void* context) {

    // to satisfy compiler
    context = context;

    while (TRUE) {

        WaitForSingleObject(trim_event, INFINITE);

        EnterCriticalSection(&pte_lock);

        for (PPTE trim_pte = pte_base; trim_pte < pte_base + num_ptes; trim_pte ++) {
        
            // found our page, let's trim
            if (trim_pte->memory.valid == 1) {

                page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                PULONG_PTR trim_va = va_from_pte(trim_pte);

                // unmap the va from the pa
                if (MapUserPhysicalPages (trim_va, 1, NULL) == FALSE) {

                    printf ("full_virtual_memory_test : could not unmap trim_va %p\n", trim_va);
                    
                    LeaveCriticalSection(&pte_lock);

                    return;
                }

                // set valid bit to 0
                trim_pte->memory.valid = 0;
                trim_pte->memory.frame_number = 0;

                // this is really going to end up going to modified or standby list
                // if going to put on standby, DO NOT ZERO frame number
                // if put on stanby, set standby event (calling anyone who wants a page that there is one now on standby list)
                // if put on modified, set modified event (memcopying)
                // mark trim_pte as transition_pte
                // page fault function will need to wait until this thread is finished to start consuming pages again (new event)
                list_insert(&free_list, curr_page);

                LeaveCriticalSection(&pte_lock);

            }
        }

        LeaveCriticalSection(&pte_lock);
    }

}

