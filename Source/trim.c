#include <stdio.h>
#include <Windows.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"

HANDLE modify_event;

// TS: have local PTE each time you update a PTE (in every file)

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

                }

                new_contents.transition.frame_number = trim_pte->memory.frame_number;
                new_contents.transition.rescuable = 1;
                *trim_pte = new_contents;

                // TS: standby lock in future
                list_insert(&standby_list, curr_page);

                #if 0
                list_insert(&modified_list, curr_page);
                SetEvent(disk_write_event);
                #endif

            }
        }

        LeaveCriticalSection(&pte_lock);
        SetEvent(fault_event);
    }

}


#define FREE 0
#define IN_USE 1


// goal here is to copy the contents to disk and then redistribute the physical frame via the standby list
void disk_write_thread(void* context) {

    context = context;

    while (TRUE) {

    // Use for loop to go through all modified pages?

        WaitForSingleObject(disk_write_event, INFINITE);

        mod_page_va = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);

        if (mod_page_va == NULL) {
            //TS: fix this
            printf("Could not allocate mod page va\n");

            return;

        }

        //EnterCriticalSection(&pte_lock);
        // TS: going to need modified lock
        page_t* curr_page = list_pop(&modified_list);

    
        if (curr_page == NULL) {

            printf("Could not pop from modified list\n");

            return;

        }

        unsigned i;

        for (i = 0; i < PAGEFILE_BLOCKS; i ++) {

            if (pagefile_state[i] == FREE) {

                pagefile_state[i] = IN_USE;

                break;

            }

        }

        if (i == PAGEFILE_BLOCKS) {

            list_insert(&modified_list, curr_page);

        }

        ULONG64 old_pfn = curr_page->pte->transition.frame_number;

        if (MapUserPhysicalPages (mod_page_va, 1, &old_pfn) == FALSE) {

            printf("full_virtual_memory_test : could not map VA %p to page %llX\n", mod_page_va, old_pfn);

            DebugBreak();

        }

        memcpy(&pagefile_contents[i * PAGE_SIZE], mod_page_va, PAGE_SIZE);

        if (MapUserPhysicalPages(mod_page_va, 1, NULL) == FALSE) {

            printf("full_virtual_memory_test : could not unmap VA %p\n", mod_page_va);

            DebugBreak();

        }

        curr_page->pte->disk.disk_address = i;
        curr_page->pte->disk.on_disc = 1;
        curr_page->pte->disk.accessed = 1;
        // TS: standby lock
        list_insert(&standby_list, curr_page);

        //LeaveCriticalSection(&pte_lock);

        //SetEvent(fault_event);

    }

}

