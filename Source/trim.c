#include <stdio.h>
#include <Windows.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"

HANDLE modify_event;

void trim_thread(void* context) {

    // to satisfy compiler
    context = context;

    while (TRUE) {

        WaitForSingleObject(trim_event, INFINITE);

        EnterCriticalSection(&pte_lock);

        for (PPTE trim_pte = pte_base; trim_pte < pte_base + num_ptes; trim_pte ++) {
        
            if (trim_pte->memory.valid == 1) {

                page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                trim_pte->transition.always_zero = 0;
                trim_pte->transition.always_zero2 = 0;

                list_insert(&standby_list, curr_page);

                // Set all valid ptes to transition (they will be added to modified list)
                #if 0
                page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                trim_pte->transition.always_zero = 0;
                trim_pte->transition.frame_number = trim_pte->memory.frame_number;
                trim_pte->transition.always_zero2 = 0;

                list_insert(&modified_list, curr_page);

                SetEvent(modify_event);
                #endif

                #if 0
                page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                trim_pte->transition.always_zero = 0;
                trim_pte->transition.always_zero2 = 0;
                trim_pte->transition.frame_number = trim_pte->memory.frame_number;

                list_insert(&standby_list, curr_page);
                #endif

                #if 0
                page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                PULONG_PTR trim_va = va_from_pte(trim_pte);

                // unmap the va from the pa
                if (MapUserPhysicalPages (trim_va, 1, NULL) == FALSE) {

                    printf ("full_virtual_memory_test : could not unmap trim_va %p\n", trim_va);

                    DebugBreak();
                    
                    continue;

                }

                // set valid bit to 0
                trim_pte->memory.valid = 0;
                trim_pte->memory.frame_number = 0;

                // if going to put on standby, DO NOT ZERO frame number
                // if put on standby, set standby event (calling anyone who wants a page that there is one now on standby list)
                // page fault function will need to wait until this thread is finished to start consuming pages again (new event)
                list_insert(&free_list, curr_page);
                #endif

            }
        }

        LeaveCriticalSection(&pte_lock);
        SetEvent(fault_event);
    }

}

// goal here is to copy the contents to disk and then redistribute the physical frame via the standby list
void disk_write_thread(void* context) {

    context = context;

    while (TRUE) {

        WaitForSingleObject(disk_write_event, INFINITE);

    }

}

