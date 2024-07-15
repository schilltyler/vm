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

                // making local transition pte so that we don't have 
                // any bits bleed over from other pte formats

                PTE new_contents;

                new_contents.entire_format = 0;

                page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                PULONG_PTR trim_va = va_from_pte(trim_pte);
                
                if (MapUserPhysicalPages(trim_va, 1, NULL) == FALSE) {

                    printf ("full_virtual_memory_test : could not unmap trim_va %p\n", trim_va);

                    DebugBreak();

                }

                new_contents.transition.frame_number = trim_pte->memory.frame_number;
                new_contents.transition.rescuable = 1;
                
                *trim_pte = new_contents;

                EnterCriticalSection(&mod_lock);
                curr_page->list_type = MODIFIED;
                list_insert(&modified_list, curr_page);
                LeaveCriticalSection(&mod_lock);

            }
        }

        LeaveCriticalSection(&pte_lock);
        SetEvent(disk_write_event);
    }

}


#define DISK_BLOCK_FREE 0
#define DISK_BLOCK_IN_USE 1

// goal here is to copy the contents to disk and then redistribute the physical frame via the standby list
void disk_write_thread(void* context) {

    context = context;

    while (TRUE) {

        WaitForSingleObject(disk_write_event, INFINITE);

        // TS: this modified list size could change because we don't have lock
        // #define for amount to write
        for (int i = 0; i < modified_list.list_size; i ++) {
            
            EnterCriticalSection(&mod_lock);
            page_t* curr_page = list_pop(&modified_list);

            

            if (curr_page == NULL) {

                LeaveCriticalSection(&mod_lock);

                printf("Could not pop from modified list\n");

                break;

            }

            unsigned i;

            for (i = 0; i < pagefile_blocks; i ++) {

                if (pagefile_state[i] == DISK_BLOCK_FREE) {

                    pagefile_state[i] = DISK_BLOCK_IN_USE;

                    break;

                }

            }

            if (i == pagefile_blocks) {

                list_insert(&modified_list, curr_page);
                LeaveCriticalSection(&mod_lock);

                continue;
                
            }

            
            ULONG64 old_pfn = pfn_from_page(curr_page, pfn_base);

            if (MapUserPhysicalPages (mod_page_va, 1, &old_pfn) == FALSE) {

                printf("full_virtual_memory_test : could not map mod VA %p to page %llX\n", mod_page_va, old_pfn);

                DebugBreak();

            }

            memcpy(&pagefile_contents[i * PAGE_SIZE], mod_page_va, PAGE_SIZE);

            if (MapUserPhysicalPages(mod_page_va, 1, NULL) == FALSE) {

                printf("full_virtual_memory_test : could not unmap mod VA %p\n", mod_page_va);

                DebugBreak();

            }

            curr_page->disk_address = i;

            EnterCriticalSection(&standby_lock);
            list_insert(&standby_list, curr_page);
            curr_page->list_type = STANDBY;

            LeaveCriticalSection(&standby_lock);
            LeaveCriticalSection(&mod_lock);

            SetEvent(fault_event);

        }

    }    

}

