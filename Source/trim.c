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

        /**
         * TS:
         * Waitformultipleobjects
         * parent thread can signal this thread to die
         * pass function an array of events
         * function will return which event was actually called
         * put code to return if the event is the kill event
         * send an event back to the parent saying that we've exited
         */

        DWORD event;

        event = WaitForMultipleObjects(2, g_trim_handles, FALSE, INFINITE);

        if (event == 0) {

            /**
             * TS:
             * try batching
             */
            for (int i = 0; i < NUM_PTE_REGIONS; i ++) {

                CRITICAL_SECTION* pte_lock = &g_pagetable->pte_lock_sections[i].lock;
                EnterCriticalSection(pte_lock);

                for (int j = i * NUM_PTES_PER_REGION; j < (i + 1) * NUM_PTES_PER_REGION; j ++) {

                    PPTE trim_pte = g_pte_base + j;

                    if (trim_pte->memory.valid == 1) {

                        /**
                         * making local transition pte so that we don't have 
                         * any bits bleed over from other pte formats
                         */
                        PTE new_contents;

                        new_contents.entire_format = 0;

                        page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, g_pfn_base);

                        PULONG_PTR trim_va = va_from_pte(trim_pte);
                        
                        if (MapUserPhysicalPages(trim_va, 1, NULL) == FALSE) {

                            printf ("full_virtual_memory_test : could not unmap trim_va %p\n", trim_va);

                            DebugBreak();

                        }

                        new_contents.transition.frame_number = trim_pte->memory.frame_number;
                        new_contents.transition.rescuable = 1;
                        
                        *trim_pte = new_contents;

                        EnterCriticalSection(&g_mod_lock);

                        curr_page->list_type = MODIFIED;
                        list_insert(&g_modified_list, curr_page);

                        LeaveCriticalSection(&g_mod_lock);

                    }

                }

                LeaveCriticalSection(pte_lock);

            }

            SetEvent(g_disk_write_event);

        }

        else {

            SetEvent(g_trim_finished_event);

            return;

        }
    }
}


#define DISK_BLOCK_FREE 0
#define DISK_BLOCK_IN_USE 1

void disk_write_thread(void* context) {

    context = context;

    while (TRUE) {

        WaitForSingleObject(g_disk_write_event, INFINITE);

        /**
         * TS:
         * Could use #define for amount to write
         */
        EnterCriticalSection(&g_mod_lock);

        for (int i = 0; i < g_modified_list.list_size; i ++) {
            
            page_t* curr_page = list_pop(&g_modified_list);

            if (curr_page == NULL) {

                printf("Could not pop from modified list\n");

                break;

            }

            unsigned j;

            for (j = 0; j < g_pagefile_blocks; j ++) {

                if (g_pagefile_state[j] == DISK_BLOCK_FREE) {

                    g_pagefile_state[j] = DISK_BLOCK_IN_USE;

                    break;

                }

            }

            if (j == g_pagefile_blocks) {

                list_insert(&g_modified_list, curr_page);

                /**
                 * TS:
                 * Anyone who frees pagefile spot, setevent for mod writer
                 */
                break;
                
            }

            
            ULONG64 old_pfn = pfn_from_page(curr_page, g_pfn_base);

            curr_page->write_in_progress = TRUE;

            LeaveCriticalSection(&g_mod_lock);

            if (MapUserPhysicalPages (g_mod_page_va, 1, &old_pfn) == FALSE) {

                printf("full_virtual_memory_test : could not map mod VA %p to page %llX\n", g_mod_page_va, old_pfn);

                DebugBreak();

            }

            memcpy(&g_pagefile_contents[j * PAGE_SIZE], g_mod_page_va, PAGE_SIZE);

            if (MapUserPhysicalPages(g_mod_page_va, 1, NULL) == FALSE) {

                printf("full_virtual_memory_test : could not unmap mod VA %p\n", g_mod_page_va);

                DebugBreak();

            }

            EnterCriticalSection(&g_mod_lock);
            curr_page->write_in_progress = FALSE;

            /**
             * TS:
             * Explain this state machine
             */
            if (curr_page->was_rescued == TRUE) {
                
                /**
                 * TS:
                 * Check was modified bit
                 * This will tell us if the user wrote to the page when
                 * they rescued, or if they just read it
                 */
                g_pagefile_state[j] = DISK_BLOCK_FREE;
                curr_page->was_rescued = FALSE;

            }
            else {

                #if 0
                /**
                 * Replenish the free list to take contention off of
                 * standby
                 * If the free list is below 10% of its full capacity
                 * Do not need free list lock here unless there is only
                 * 1 page on the free list because we are inserting at
                 * the tail and it is popping from the head
                 * Probably just better to acquire the lock to be safe
                 * for now
                 */
                if (g_free_list.list_size < g_physical_page_count * 0.1) {

                    EnterCriticalSection(&g_free_lock);
                    list_insert_tail(g_free_list, curr_page);
                    curr_page->list_type = FREE;
                    LeaveCriticalSection(&g_free_lock);

                    SetEvent(g_fault_event);

                }
                else {
                
                    // place current standby code
                
                }
                #endif

                curr_page->disk_address = j;

                EnterCriticalSection(&g_standby_lock);
                list_insert(&g_standby_list, curr_page);
                curr_page->list_type = STANDBY;

                LeaveCriticalSection(&g_standby_lock);

                SetEvent(g_fault_event);

            }

        }

        LeaveCriticalSection(&g_mod_lock);

    } 

}

