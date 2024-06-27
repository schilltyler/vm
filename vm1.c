// Includes
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <excpt.h>
#include "./page.h"
#include "./pageTable.h"
//#include "./globals.h"

// Linker
#pragma comment(lib, "advapi32.lib")


// Privileges (do not worry about)
BOOL
GetPrivilege  (
    VOID
    )
{
    struct {
        DWORD Count;
        LUID_AND_ATTRIBUTES Privilege [1];
    } Info;

    //
    // This is Windows-specific code to acquire a privilege.
    // Understanding each line of it is not so important for
    // our efforts.
    //

    HANDLE hProcess;
    HANDLE Token;
    BOOL Result;

    //
    // Open the token.
    //

    hProcess = GetCurrentProcess ();

    Result = OpenProcessToken (hProcess,
                               TOKEN_ADJUST_PRIVILEGES,
                               &Token);

    if (Result == FALSE) {
        printf ("Cannot open process token.\n");
        return FALSE;
    }

    //
    // Enable the privilege. 
    //

    Info.Count = 1;
    Info.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;

    //
    // Get the LUID.
    //

    Result = LookupPrivilegeValue (NULL,
                                   SE_LOCK_MEMORY_NAME,
                                   &(Info.Privilege[0].Luid));

    if (Result == FALSE) {
        printf ("Cannot get privilege\n");
        return FALSE;
    }

    //
    // Adjust the privilege.
    //

    Result = AdjustTokenPrivileges (Token,
                                    FALSE,
                                    (PTOKEN_PRIVILEGES) &Info,
                                    0,
                                    NULL,
                                    NULL);

    //
    // Check the result.
    //

    if (Result == FALSE) {
        printf ("Cannot adjust token privileges %u\n", GetLastError ());
        return FALSE;
    } 

    if (GetLastError () != ERROR_SUCCESS) {
        printf ("Cannot enable the SE_LOCK_MEMORY_NAME privilege - check local policy\n");
        return FALSE;
    }

    CloseHandle (Token);

    return TRUE;
}


// Global variables
PTE* pte_base;
PULONG_PTR vmem_base;
page_t* pfn_base;
listhead_t free_list;

/*
 * Third (full) test
*/
VOID
full_virtual_memory_test (
    VOID
    )
{
    // instantiate variables
    unsigned i;
    PULONG_PTR arbitrary_va;
    unsigned random_number;
    BOOL allocated;
    BOOL page_faulted;
    BOOL privilege;
    BOOL obtained_pages;
    ULONG_PTR physical_page_count;
    PULONG_PTR physical_page_numbers;
    HANDLE physical_page_handle;
    ULONG_PTR virtual_address_size;
    ULONG_PTR virtual_address_size_in_unsigned_chunks;

    // get privilege to control physical pages
    privilege = GetPrivilege ();

    if (privilege == FALSE) {
        printf ("full_virtual_memory_test : could not get privilege\n");
        return;
    }    

    physical_page_handle = GetCurrentProcess ();

    physical_page_count = NUMBER_OF_PHYSICAL_PAGES;

    // make array for physical page numbers
    physical_page_numbers = malloc (physical_page_count * sizeof (ULONG_PTR));

    if (physical_page_numbers == NULL) {
        printf ("full_virtual_memory_test : could not allocate array to hold physical page numbers\n");
        return;
    }

    // get the physical pages
    allocated = AllocateUserPhysicalPages (physical_page_handle,
                                           &physical_page_count,
                                           physical_page_numbers);

    if (allocated == FALSE) {
        printf ("full_virtual_memory_test : could not allocate physical pages\n");
        return;
    }

    // could not get all of the physical pages asked for
    if (physical_page_count != NUMBER_OF_PHYSICAL_PAGES) {

        printf ("full_virtual_memory_test : allocated only %llu pages out of %u pages requested\n",
                physical_page_count,
                NUMBER_OF_PHYSICAL_PAGES);
    }

    // reserve user address space region
    // lets us connect physical pages to virtual pages
    // larger than physical memory
    virtual_address_size = 64 * physical_page_count * PAGE_SIZE;

    // Round down to a PAGE_SIZE boundary
    virtual_address_size &= ~PAGE_SIZE;

    virtual_address_size_in_unsigned_chunks =
                        virtual_address_size / sizeof (ULONG_PTR);

    // allocate pages virtually
    vmem_base = VirtualAlloc (NULL,
                      virtual_address_size,
                      MEM_RESERVE | MEM_PHYSICAL,
                      PAGE_READWRITE);

    if (vmem_base == NULL) {
        printf ("full_virtual_memory_test : could not reserve memory\n");
        return;
    }

    ULONG_PTR num_ptes = virtual_address_size / PAGE_SIZE;

    // initialize PTE's we will use
    ULONG_PTR num_pte_bytes = num_ptes * sizeof(PTE);

    pte_base = malloc(num_pte_bytes);

    if (pte_base == NULL){
        printf("Could not malloc pte_base\n");
        return;
    }

    memset(pte_base, 0, num_pte_bytes);

    // create free list, then populate it with physical frame numbers
    free_list.flink = &free_list;
    free_list.blink = &free_list;

    ULONG64 high_pfn = 0x0;
    for (int i = 0; i < physical_page_count; i ++) {
        if (physical_page_numbers[i] > high_pfn) {
            high_pfn = physical_page_numbers[i];
        }
    }

    pfn_base = VirtualAlloc(NULL, high_pfn * sizeof(page_t), MEM_RESERVE, PAGE_READWRITE);

    if (pfn_base == NULL) {
        printf("Could not allocate pfn_base\n");
        return;
    }

    for (int i = 0; i < physical_page_count; i ++) {

        page_t* new_page = page_create(pfn_base, physical_page_numbers[i]);
        list_insert(&free_list, new_page);

    }


    // Now perform random accesses.
    srand (time (NULL));

    for (i = 0; i < MB (1); i += 1) {
        // Randomly access different portions of virtual address space
        // If never accessed page, will page fault
        // CPU will get physical page and install PTE to map it
        // CPU will repeat instruction, will see valid PTE, will get contents

        random_number = rand () * rand() * rand();

        random_number %= virtual_address_size_in_unsigned_chunks;

        // Write the virtual address into each page.  If we need to
        // debug anything, we'll be able to see these in the pages.

        page_faulted = FALSE;

        arbitrary_va = vmem_base + random_number;

        _try {

            *arbitrary_va = (ULONG_PTR) arbitrary_va;

        } _except (EXCEPTION_EXECUTE_HANDLER) {

            page_faulted = TRUE;
        }

        // We've page faulted, now go through states to grab the right memory page
        if (page_faulted) {

            // try and get page from free list
            page_t* free_page = list_pop(&free_list);

            // free list does not have any pages left
            // so . . . trim random active page
            if (free_page == NULL) {

                // start a trimming thread
                #if 0
                HANDLE threads[1];
                PARAM_STRUCT params;
                params.test_type = test;
                params.state = 0;
                threads[0] = CreateThread(NULL, 0, trim_thread, &params, 0, NULL);
                WaitForSingleObject(threads[0], INFINITE);
                CloseHandle(threads[0]);
                #endif
                
                for (PPTE trim_pte = pte_base; trim_pte < pte_base + num_ptes; trim_pte ++) {
                    
                    // found our page, let's trim
                    if (trim_pte->memory.valid == 1) {

                        page_t* curr_page = page_from_pfn(trim_pte->memory.frame_number, pfn_base);

                        list_insert(&free_list, curr_page);

                        PULONG_PTR trim_va = va_from_pte(trim_pte);

                        // unmap the va from the pa
                        if (MapUserPhysicalPages (trim_va, 1, NULL) == FALSE) {

                            printf ("full_virtual_memory_test : could not unmap trim_va %p\n", trim_va);

                            return;
                        }

                        // set valid bit to 0
                        trim_pte->memory.valid = 0;
                        trim_pte->memory.frame_number = 0;

                        free_page = list_pop(&free_list);

                        if (free_page == NULL) {
                            printf("Could not pop from free_list\n");
                        }

                        break;

                    }

                }    

            }

            ULONG64 pfn = pfn_from_page(free_page, pfn_base);

            if (MapUserPhysicalPages (arbitrary_va, 1, &pfn) == FALSE) {

                printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", arbitrary_va, pfn);

                return;
            }

            PPTE pte = pte_from_va(arbitrary_va);

            pte->memory.valid = 1;
            pte->memory.frame_number = pfn;

            // need this for when I trim page from something like standby and want to cut off old pte to replace with new one
            free_page->pte = pte;

            // No exception handler needed now since we have connected
            // the virtual address above to one of our physical pages
            // so no subsequent fault can occur.
            *arbitrary_va = (ULONG_PTR) arbitrary_va;
            
            // now make pte to show this link
             
            
            // TS: use this for trim
            #if 0
            //
            // Unmap the virtual address translation we installed above
            // now that we're done writing our value into it.
            //

            if (MapUserPhysicalPages (arbitrary_va, 1, NULL) == FALSE) {

                printf ("full_virtual_memory_test : could not unmap VA %p\n", arbitrary_va);

                return;
            }
            #endif

        }
    }

    printf ("full_virtual_memory_test : finished accessing %u random virtual addresses\n", i);

    //
    // Now that we're done with our memory we can be a good
    // citizen and free it.
    //

    VirtualFree (vmem_base, 0, MEM_RELEASE);

    return;
}

VOID 
main (
    int argc,
    char** argv
    )
{
    // Test our very complicated usermode virtual implementation.
    // 
    // We will control the virtual and physical address space management
    // ourselves with the only two exceptions being that we will :
    //
    // 1. Ask the operating system for the physical pages we'll use to
    //    form our pool.
    //
    // 2. Ask the operating system to connect one of our virtual addresses
    //    to one of our physical pages (from our pool).
    //
    // We would do both of those operations ourselves but the operating
    // system (for security reasons) does not allow us to.
    //
    // But we will do all the heavy lifting of maintaining translation
    // tables, PFN data structures, management of physical pages,
    // virtual memory operations like handling page faults, materializing
    // mappings, freeing them, trimming them, writing them out to backing
    // store, bringing them back from backing store, protecting them, etc.
    //
    // This is where we can be as creative as we like, the sky's the limit !

    full_virtual_memory_test ();

    return;
}
