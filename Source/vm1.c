#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <excpt.h>
#include <time.h>
#include "../Include/initialize.h"
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/pagefault.h"
#include "../Include/trim.h"

/**
 * Linker
 */
#pragma comment(lib, "advapi32.lib")



VOID full_virtual_memory_test (VOID)
{
    clock_t timer;
    double sim_runtime;

    timer = clock();

    initialize_system();


    for (int i = 0; i < g_num_fault_threads; i ++) {

        WaitForSingleObject(g_threads[i + 2], INFINITE);

    }

    timer = clock() - timer;
    sim_runtime = (double) (timer) / CLOCKS_PER_SEC;
    
    printf ("full_virtual_memory_test : finished accessing %d random virtual addresses over %d threads\n", ACCESS_AMOUNT, g_num_fault_threads);
    printf("CPU time: %f\n", sim_runtime);
    printf("Total faults: %llu\n", g_num_faults);

    SetEvent(g_kill_trim_event);

    WaitForSingleObject(g_trim_finished_event, INFINITE);

    VirtualFree(g_vmem_base, 0, MEM_RELEASE);

}

VOID fault_thread() {

    ULONG64 timestamp;
    int fault_result;
    PULONG_PTR arbitrary_va;
    unsigned random_number;
    BOOL page_faulted;
    LPVOID mod_page_va2;
    unsigned i;

    srand (time (NULL));

    #if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE
    mod_page_va2 = VirtualAlloc2 (NULL,
                       NULL,
                       PAGE_SIZE,
                       MEM_RESERVE | MEM_PHYSICAL,
                       PAGE_READWRITE,
                       &g_vmem_parameter,
                       1);
    #else
    mod_page_va2 = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
    #endif

    arbitrary_va = NULL;

    if (VA_ITERATE_TYPE == 1) {

        for (i = 0; i < ACCESS_AMOUNT / g_num_fault_threads; i += 1) {

            page_faulted = FALSE;

            if (arbitrary_va == NULL) {

                /**
                 * This will get the cpu tick count,
                 * which is constantly being updated
                 * It will be different across all threads
                 */
                random_number = __rdtsc();

                random_number %= g_virtual_address_size_in_unsigned_chunks;

                arbitrary_va = g_vmem_base + random_number;
            }
        
            __try {

                *arbitrary_va = (ULONG_PTR) arbitrary_va;

                arbitrary_va = NULL;

            } __except (EXCEPTION_EXECUTE_HANDLER) {

                page_faulted = TRUE;
            }

            if (page_faulted) {

                fault_result = handle_page_fault(arbitrary_va, mod_page_va2);

                g_num_faults = InterlockedIncrement64(&g_num_faults);

                if (g_num_faults % KB(64) == 0) {

                    /**
                     * TS:
                     * Can also print modified, standby, and free lists
                     */
                    printf("Current num faults: %llu\n", g_num_faults);

                }

                i --;

            }
            /**
             * TS:
             * Can add else function marking va as accessed (pass in va value) here
             */
        }

        return;

    }

    else if (VA_ITERATE_TYPE == 2){

        /**
         * Sequential va's
         */
    
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

            if (page_faulted) {

                fault_result = handle_page_fault(arbitrary_va, mod_page_va2);

                i --;

            }
            // else function marking va as accessed (pass in va value)
        }

        return;

    }

}


VOID main (int argc, char** argv)
{
    /**
     * First command line parameter is num fault threads
     */
    g_num_fault_threads = atoi(argv[1]);

    full_virtual_memory_test();

    return;
}
