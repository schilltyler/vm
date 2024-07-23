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

        WaitForSingleObject(g_trim_event, INFINITE);

        // TS: 128 is the number of pte regions. Should prob make this variable
        for (int i = 0; i < 128; i ++) {

            CRITICAL_SECTION* pte_lock = &g_pagetable->pte_lock_sections[i].lock;
            EnterCriticalSection(pte_lock);

            // TS: 32 is num ptes per region, should prob make this variable
            for (int j = i * 32; j < (i + 1) * 32; j ++) {

                PPTE trim_pte = g_pte_base + j;

                if (trim_pte->memory.valid == 1) {

                    // making local transition pte so that we don't have 
                    // any bits bleed over from other pte formats

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

}


#define DISK_BLOCK_FREE 0
#define DISK_BLOCK_IN_USE 1

// goal here is to copy the contents to disk and then redistribute the physical frame via the standby list
void disk_write_thread(void* context) {

    context = context;

    while (TRUE) {

        WaitForSingleObject(g_disk_write_event, INFINITE);

        // TS: this modified list size could change because we don't have lock
        // #define for amount to write
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

                // TS: anyone who frees pagefile spot, setevent for mod writer
                break;
                
            }

            
            ULONG64 old_pfn = pfn_from_page(curr_page, g_pfn_base);

            if (MapUserPhysicalPages (g_mod_page_va, 1, &old_pfn) == FALSE) {

                printf("full_virtual_memory_test : could not map mod VA %p to page %llX\n", g_mod_page_va, old_pfn);

                DebugBreak();

            }

            memcpy(&g_pagefile_contents[j * PAGE_SIZE], g_mod_page_va, PAGE_SIZE);

            if (MapUserPhysicalPages(g_mod_page_va, 1, NULL) == FALSE) {

                printf("full_virtual_memory_test : could not unmap mod VA %p\n", g_mod_page_va);

                DebugBreak();

            }

            curr_page->disk_address = j;

            EnterCriticalSection(&g_standby_lock);
            list_insert(&g_standby_list, curr_page);
            curr_page->list_type = STANDBY;

            LeaveCriticalSection(&g_standby_lock);

            SetEvent(g_fault_event);

        }

        LeaveCriticalSection(&g_mod_lock);

    } 

}

