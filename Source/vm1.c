// Includes
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <excpt.h>
#include "../Include/initialize.h"
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/pagefault.h"
#include "../Include/trim.h"

// Linker
#pragma comment(lib, "advapi32.lib")


VOID full_virtual_memory_test (VOID)
{
    initialize_system();

    // create standby list, not populated yet though
    standby_list.flink = &standby_list;
    standby_list.blink = &standby_list;

    // create modified list, not populated yet though
    modified_list.flink = &modified_list;
    modified_list.blink = &modified_list;

    // create free list, then populate it with physical frame numbers
    free_list.flink = &free_list;
    free_list.blink = &free_list;

    for (int i = 0; i < physical_page_count; i ++) {

        page_t* new_page = page_create(pfn_base, physical_page_numbers[i]);
        list_insert(&free_list, new_page);

    }


    // Now perform random accesses.
    srand (time (NULL));
    int fault_result;

    // TS: make this a local variable (when you have multiple page fault threads this could get complicated as a global)
    arbitrary_va = NULL;

    for (i = 0; i < MB (1); i += 1) {

        page_faulted = FALSE;

        if (arbitrary_va == NULL) {

            random_number = rand () * rand() * rand();

            random_number %= virtual_address_size_in_unsigned_chunks;

            arbitrary_va = vmem_base + random_number;
        }
       
        _try {

            *arbitrary_va = (ULONG_PTR) arbitrary_va;

            arbitrary_va = NULL;

        } _except (EXCEPTION_EXECUTE_HANDLER) {

            page_faulted = TRUE;
        }

        // We've page faulted, now go through states to grab the right memory page
        if (page_faulted) {

            fault_result = handle_page_fault(arbitrary_va);

            i --;

        }
        // else function marking va as accessed (pass in va value)
    }

    printf ("full_virtual_memory_test : finished accessing %u random virtual addresses\n", i);

    VirtualFree (vmem_base, 0, MEM_RELEASE);

    return;
}


VOID main (int argc, char** argv)
{
    full_virtual_memory_test ();

    return;
}
