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

    num_faults = 0;


    for (int i = 0; i < num_fault_threads; i ++) {

        WaitForSingleObject(threads[i + 2], INFINITE);

    }

    printf ("full_virtual_memory_test : finished accessing %d random virtual addresses over %d threads\n", (1024 * 1024) * num_fault_threads, num_fault_threads);
    printf("Number of faults: %d\n", num_faults);

    VirtualFree (vmem_base, 0, MEM_RELEASE);

}

VOID fault_thread() {

    // Now perform random accesses.
    
    // TS: could be causing the multiple page faults of the same va
    srand (time (NULL));

    int fault_result;
    PULONG_PTR arbitrary_va;
    unsigned random_number;
    BOOL page_faulted;

    // TS: local num_faults counter, send back to parent thread and then add them up from all threads
    // maybe use threads typdef struct to have specific variables that you track

    arbitrary_va = NULL;

    unsigned i;

    if (va_iterate_type == 1) {

        for (i = 0; i < 1024 * 1024; i += 1) {

            page_faulted = FALSE;

            if (arbitrary_va == NULL) {

                random_number = rand () * rand() * rand();

                random_number %= virtual_address_size_in_unsigned_chunks;

                arbitrary_va = vmem_base + random_number;
            }
        
            __try {

                *arbitrary_va = (ULONG_PTR) arbitrary_va;

                arbitrary_va = NULL;

            } __except (EXCEPTION_EXECUTE_HANDLER) {

                page_faulted = TRUE;
            }

            // We've page faulted, now go through states to grab the right memory page
            if (page_faulted) {

                fault_result = handle_page_fault(arbitrary_va);

                num_faults ++;

                i --;

            }
            // else function marking va as accessed (pass in va value)
        }

        return;

    }

    // sequential
    else if (va_iterate_type == 2){
    
        for (i = 0; i < 1024 * 1024; i += 1) {

            page_faulted = FALSE;

            if (arbitrary_va == NULL) {

                // TS: fix this, because I don't believe it is right
                arbitrary_va = vmem_base + i;
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

                num_faults ++;

                i --;

            }
            // else function marking va as accessed (pass in va value)
        }

        return;

    }

}


VOID main (int argc, char** argv)
{
    // TS: how do I make va_iterate_type a string, couldn't figure it out before

    virtual_address_size = strtoul(argv[1], NULL, 10) * 1024 * 1024;
    physical_page_count = strtoul(argv[2], NULL, 10);
    num_fault_threads = atoi(argv[3]);
    va_iterate_type = atoi(argv[4]);


    number_of_physical_pages = ((virtual_address_size / PAGE_SIZE) / 64);
    pagefile_blocks = ((virtual_address_size / PAGE_SIZE) - number_of_physical_pages + 1);

    full_virtual_memory_test();

    return;
}
