// Includes
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <excpt.h>
#include "./page.h"
#include "./pageTable.h"

// Linker
#pragma comment(lib, "advapi32.lib")

// Recurring definitions
#define PAGE_SIZE                   4096
#define MB(x)                       ((x) * 1024 * 1024)
#define VIRTUAL_ADDRESS_SIZE        MB(16)
#define VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS        (VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR))
#define NUMBER_OF_PHYSICAL_PAGES   ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / 64) // ~1% of virtual address space


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

    PPTE pte_base = malloc(num_pte_bytes);

    memset(pte_base, 0, num_pte_bytes);

    // create free list, then populate it with physical frame numbers
    listhead_t free_list;
    free_list.flink = &free_list;
    free_list.blink = &free_list;

    // this is the first page that we can always have on hand (links to all other pages)
    page_t* first_page = page_create(physical_page_numbers[0]);
    list_insert(&free_list, first_page);

    // create all of the pfn entries (page structs) first
    for (int i = 1; i < physical_page_count; i ++) {
        page_create(physical_page_numbers[i]);
    }

    // now add all of the pages to the free list
    page_t* new_page = first_page;

    for (int i = 1; i < physical_page_count; i ++) {

        list_insert(&free_list, new_page);
        new_page = new_page->flink;

    }


    if (pte_base == NULL) {
            printf("Could not allocate pte space\n");
            return;
    }

    // Now perform random accesses.
    srand (time (NULL));

    for (i = 0; i < MB (1); i += 1) {
        // Randomly access different portions of virtual address space
        // If never accessed page, will page fault
        // CPU will get physical page and install PTE to map it
        // CPU will repeat instruction, will see valid PTE, will get contents

        random_number = rand () * rand() * rand(); // this could be too low

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
        // Free list is not going to be an option after short amount of time
        // Go through pfn's and find the right aged page
        if (page_faulted) {

            // connect virtual and physical addresses now (set the valid bit)
            // 1) look at PTE for this va (might be trimmed, first access, etc.)
            // 2) remove page from free frames list if available
            // 3) modify PTE to represent physical frame we just allocated

            // try and get page from free list
            page_t* free_page = list_pop(&free_list);

            // free list does not have any pages left
            // so . . . trim random active page
            if (free_page == NULL) {
                
                // loop over pte's starting from pte_base (for loop, increase by 1 from base)
                // valid bit equal to 1
                
                for (PPTE trim_pte = pte_base; trim_pte < pte_base + num_ptes; trim_pte ++) {
                    
                    // found our page, let's trim
                    if (trim_pte->memory.valid == 1) {

                        // get the physical frame number from the pte, add it back to free list
                        // will circulate back and find a new virtual address that needs it
                        // second parameter should be a page_t* that was already created before this
                        // need to change around page_create and insert functions
                        // page_create separate from list_insert
                        // go through page_t to find the one with matching frame number
                        // what page do we start at?
                        page_t* curr_page = first_page;

                        for (int i = 0; i < physical_page_count; i ++) {
                            
                            // TS: run pfn_from_frame_number??
                            if (curr_page->pfn == trim_pte->memory.frame_number) {
                                break;
                            }

                            curr_page = curr_page->flink;

                        }

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

                        break;

                    }

                }    

            }

            if (MapUserPhysicalPages (arbitrary_va, 1, (PULONG_PTR) &free_page->pfn) == FALSE) {

                printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", arbitrary_va, free_page->pfn);

                return;
            }

            PPTE pte = pte_from_va(arbitrary_va);

            pte->memory.valid = 1;
            pte->memory.frame_number = free_page->pfn;
    

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
