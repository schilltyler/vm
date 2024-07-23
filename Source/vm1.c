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

    g_num_faults = 0;


    for (int i = 0; i < g_num_fault_threads; i ++) {

        WaitForSingleObject(g_threads[i + 2], INFINITE);

    }

    printf ("full_virtual_memory_test : finished accessing %d random virtual addresses over %d threads\n", (1024 * 1024) * g_num_fault_threads, g_num_fault_threads);
    printf("Number of faults: %d\n", g_num_faults);

    VirtualFree (g_vmem_base, 0, MEM_RELEASE);

}

VOID fault_thread() {
    
    // TS: could be causing the multiple page faults of the same va
    srand (time (NULL));

    int fault_result;
    PULONG_PTR arbitrary_va;
    unsigned random_number;
    BOOL page_faulted;
    LPVOID mod_page_va2;

    mod_page_va2 = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);

    // TS: local num_faults counter, send back to parent thread and then add them up from all threads
    // maybe use threads typdef struct to have specific variables that you track

    arbitrary_va = NULL;

    unsigned i;

    if (g_va_iterate_type == 1) {

        // TS: divide 1 mb by num_fault_threads

        for (i = 0; i < (1024 * 1024) / g_num_fault_threads; i += 1) {

            page_faulted = FALSE;

            if (arbitrary_va == NULL) {

                random_number = rand () * rand() * rand();

                random_number %= g_virtual_address_size_in_unsigned_chunks;

                arbitrary_va = g_vmem_base + random_number;
            }
        
            __try {

                *arbitrary_va = (ULONG_PTR) arbitrary_va;

                arbitrary_va = NULL;

            } __except (EXCEPTION_EXECUTE_HANDLER) {

                page_faulted = TRUE;
            }

            // We've page faulted, now go through states to grab the right memory page
            if (page_faulted) {

                fault_result = handle_page_fault(arbitrary_va, mod_page_va2);

                g_num_faults ++;

                i --;

            }
            // else function marking va as accessed (pass in va value)
        }

        return;

    }

    // sequential
    else if (g_va_iterate_type == 2){
    
        for (i = 0; i < 1024 * 1024; i += 1) {

            page_faulted = FALSE;

            if (arbitrary_va == NULL) {

                // TS: fix this, because I don't believe it is right
                arbitrary_va = g_vmem_base + i;
            }
        
            _try {

                *arbitrary_va = (ULONG_PTR) arbitrary_va;

                arbitrary_va = NULL;

            } _except (EXCEPTION_EXECUTE_HANDLER) {

                page_faulted = TRUE;
            }

            // We've page faulted, now go through states to grab the right memory page
            if (page_faulted) {

                fault_result = handle_page_fault(arbitrary_va, mod_page_va2);

                g_num_faults ++;

                i --;

            }
            // else function marking va as accessed (pass in va value)
        }

        return;

    }

}


VOID main (int argc, char** argv)
{
    g_virtual_address_size = strtoul(argv[1], NULL, 10) * 1024 * 1024;
    g_physical_page_count = strtoul(argv[2], NULL, 10);
    g_num_fault_threads = atoi(argv[3]);
    g_va_iterate_type = atoi(argv[4]);

    //number_of_physical_pages = ((virtual_address_size / PAGE_SIZE) / 2);

    full_virtual_memory_test();

    return;
}
