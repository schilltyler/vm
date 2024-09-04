#include <stdio.h>
#include <Windows.h>
#include "../Include/initialize.h"
#include "../Include/trim.h"


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "onecore.lib")

/**
 * TS:
 * Could return int for each init function
 * to make sure that each one successfully executes
 */

// Global lists
listhead_t g_free_list;
listhead_t g_standby_list;
listhead_t g_modified_list;
listhead_t g_zero_list;

// Global pte variables
ULONG_PTR g_num_ptes;
PTE* g_pte_base;
PAGE_TABLE* g_pagetable;

// Global PA variables
ULONG_PTR g_physical_page_count;
PULONG_PTR g_physical_page_numbers;
page_t* g_pfn_base;
ULONG64 g_low_pfn;

// Global VA variables
ULONG_PTR g_virtual_address_size;
ULONG_PTR g_virtual_address_size_in_unsigned_chunks;
PULONG_PTR g_vmem_base;
MEM_EXTENDED_PARAMETER g_vmem_parameter;

// Global disk-write variables
ULONG_PTR g_pagefile_blocks;
UCHAR* g_pagefile_contents;
UCHAR* g_pagefile_state;
PAGEFILE_DEBUG* g_pagefile_addresses;
LPVOID g_mod_page_va;

// Global faulting variables
int g_num_fault_threads;
int g_va_iterate_type;

// Global trim variables
PVOID* g_trim_vas;

// Global Events/Threads
HANDLE g_trim_event;
HANDLE g_disk_write_event;
HANDLE g_fault_event;
HANDLE g_kill_trim_event;
HANDLE g_trim_finished_event;
HANDLE* g_trim_handles;
HANDLE* g_threads;
VOID fault_thread();

// Global Locks
CRITICAL_SECTION g_mod_lock;
CRITICAL_SECTION g_standby_lock;
CRITICAL_SECTION g_free_lock;
CRITICAL_SECTION g_zero_lock;

// Thread stats
ULONG64 g_num_faults;

// Debugging Variables
#if CIRCULAR_LOG
PAGE_LOG g_page_log[LOG_SIZE];
volatile ULONG64 log_idx;
#endif


// Initialization globals
HANDLE physical_page_handle;

// Windows stuff
BOOL GetPrivilege (VOID)
{
    struct {
        DWORD Count;
        LUID_AND_ATTRIBUTES Privilege [1];
    } Info;

    HANDLE hProcess;
    HANDLE Token;
    BOOL Result;

    // Open the token.
    hProcess = GetCurrentProcess ();

    Result = OpenProcessToken (hProcess,
                               TOKEN_ADJUST_PRIVILEGES,
                               &Token);

    if (Result == FALSE) {
        printf ("Cannot open process token.\n");
        return FALSE;
    }

    // Enable the privilege. 
    Info.Count = 1;
    Info.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Get the LUID.
    Result = LookupPrivilegeValue (NULL,
                                   SE_LOCK_MEMORY_NAME,
                                   &(Info.Privilege[0].Luid));

    if (Result == FALSE) {
        printf ("Cannot get privilege\n");
        return FALSE;
    }

    // Adjust the privilege.
    Result = AdjustTokenPrivileges (Token,
                                    FALSE,
                                    (PTOKEN_PRIVILEGES) &Info,
                                    0,
                                    NULL,
                                    NULL);

    // Check the result.
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

HANDLE
CreateSharedMemorySection (VOID)
{
    HANDLE section;
    //
    // Create an AWE section.  Later we deposit pages into it and/or
    // return them.
    //
    g_vmem_parameter.Type = MemSectionExtendedParameterUserPhysicalFlags;
    g_vmem_parameter.ULong = 0;
    section = CreateFileMapping2 (INVALID_HANDLE_VALUE,
                                  NULL,
                                  SECTION_MAP_READ | SECTION_MAP_WRITE,
                                  PAGE_READWRITE,
                                  SEC_RESERVE,
                                  0,
                                  NULL,
                                  &g_vmem_parameter,
                                  1);
    return section;
}

VOID initialize_events(VOID) 
{
    g_trim_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_fault_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_disk_write_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    /**
     * At 16000000 the critical section api will only look at bottom
     * 20 or so bits (forget actual value) because it uses some of its
     * bits to keep debug info. This would cause it to set the spin count
     * to something we don't want. The value in there now is the correct
     * size so we are spinning a lot which is what we want.
     */
    InitializeCriticalSectionAndSpinCount(&g_mod_lock, 15999999);
    InitializeCriticalSectionAndSpinCount(&g_standby_lock, 15999999);
    InitializeCriticalSectionAndSpinCount(&g_free_lock, 15999999);
    InitializeCriticalSectionAndSpinCount(&g_zero_lock, 15999999);

    g_kill_trim_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_trim_finished_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    g_trim_handles = malloc(sizeof(HANDLE*) * 2);
    g_trim_handles[0] = g_trim_event;
    g_trim_handles[1] = g_kill_trim_event;
}

VOID initialize_threads(VOID)
{
    g_threads = (HANDLE*) malloc(sizeof(HANDLE) * (2 + g_num_fault_threads));

    if (g_threads == NULL) {

        printf("Could not malloc g_threads list\n");
        return;

    }

    /**
     * TS:
     * Add checks to make sure these threads are actually created
     */
    g_threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) trim_thread, NULL, 0, NULL);
    g_threads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) disk_write_thread, NULL, 0, NULL);

    for (int i = 0; i < g_num_fault_threads; i ++) {

        g_threads[i + 2] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) fault_thread, NULL, 0, NULL);

    }

    g_trim_vas = malloc(sizeof(PULONG_PTR) * g_num_ptes);

    if (g_trim_vas == NULL) {

        printf("Could not malloc g_trim_vas list\n");
        return;

    }

}

VOID initialize_pages(VOID)
{
    ULONG_PTR number_of_physical_pages;

    #if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE
    physical_page_handle = CreateSharedMemorySection();

    if (physical_page_handle == NULL) {
        printf ("CreateFileMapping2 failed, error %#x\n", GetLastError ());
        return;
    }
    #else
    physical_page_handle = GetCurrentProcess ();
    #endif

    g_physical_page_count = NUMBER_OF_PHYSICAL_PAGES;

    g_physical_page_numbers = malloc (g_physical_page_count * sizeof (ULONG_PTR));

    if (g_physical_page_numbers == NULL) {
        printf ("full_virtual_memory_test : could not allocate array to hold physical page numbers\n");
        return;
    }

    BOOL allocated;

    allocated = AllocateUserPhysicalPages (physical_page_handle,
                                           &g_physical_page_count,
                                           g_physical_page_numbers);

    if (allocated == FALSE) {
        printf ("full_virtual_memory_test : could not allocate physical pages\n");
        return;
    }

    if (g_physical_page_count != NUMBER_OF_PHYSICAL_PAGES) {

        printf ("full_virtual_memory_test : allocated only %llu pages out of %u pages requested\n",
                g_physical_page_count,
                NUMBER_OF_PHYSICAL_PAGES);
    }

    g_pagefile_blocks = ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) - g_physical_page_count + 1);
}

VOID initialize_user_va_space(VOID) 
{
    g_virtual_address_size = VIRTUAL_ADDRESS_SIZE;

    /**
     * TS:
     * Explanation for this:
     * Round down to a PAGE_SIZE boundary
     */
    g_virtual_address_size &= ~PAGE_SIZE;

    g_virtual_address_size_in_unsigned_chunks =
                        g_virtual_address_size / sizeof (ULONG_PTR);

    #if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE

    /**
     * Allocate a MEM_PHYSICAL region that is "connected" to the AWE section
     * created above.
     */
    g_vmem_parameter.Type = MemExtendedParameterUserPhysicalHandle;
    g_vmem_parameter.Handle = physical_page_handle;

    g_vmem_base = VirtualAlloc2 (NULL,
                       NULL,
                       g_virtual_address_size,
                       MEM_RESERVE | MEM_PHYSICAL,
                       PAGE_READWRITE,
                       &g_vmem_parameter,
                       1);
    #else
    g_vmem_base = VirtualAlloc (NULL,
                      g_virtual_address_size,
                      MEM_RESERVE | MEM_PHYSICAL,
                      PAGE_READWRITE);
    #endif

    if (g_vmem_base == NULL) {
        printf ("full_virtual_memory_test : could not reserve memory\n");
        return;
    }
}

VOID initialize_pte_metadata(VOID) 
{
    g_num_ptes = VIRTUAL_ADDRESS_SIZE / PAGE_SIZE;

    ULONG_PTR num_pte_bytes = g_num_ptes * sizeof(PTE);

    g_pte_base = malloc(num_pte_bytes);

    if (g_pte_base == NULL){
        printf("Could not malloc pte_base\n");
        return;
    }

    memset(g_pte_base, 0, num_pte_bytes);

    g_pagetable = create_pagetable();
}

VOID initialize_pfn_metadata(VOID)
{
    ULONG64 high_pfn = 0x0;
    
    g_low_pfn = MAXULONG64;

    for (int i = 0; i < g_physical_page_count; i ++) {

        if (g_physical_page_numbers[i] > high_pfn) {
            high_pfn = g_physical_page_numbers[i];
        }

        if (g_physical_page_numbers[i] < g_low_pfn) {
            g_low_pfn = g_physical_page_numbers[i];
        }
    }

    g_pfn_base = VirtualAlloc(NULL, high_pfn * sizeof(page_t), MEM_RESERVE, PAGE_READWRITE);

    if (g_pfn_base == NULL) {
        printf("Could not allocate pfn_base\n");
        return;
    }
}

VOID initialize_mod_va_space(VOID) 
{
    #if SUPPORT_MULTIPLE_VA_TO_SAME_PAGE
    g_mod_page_va = VirtualAlloc2 (NULL,
                       NULL,
                       PAGE_SIZE,
                       MEM_RESERVE | MEM_PHYSICAL,
                       PAGE_READWRITE,
                       &g_vmem_parameter,
                       1);
    #else
    g_mod_page_va = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
    #endif

    if (g_mod_page_va == NULL) {
    
        printf("Could not allocate mod page va\n");

        return;

    }

    g_pagefile_contents = malloc(g_pagefile_blocks * PAGE_SIZE);

    if (g_pagefile_contents == NULL) {
        
        printf("Could not malloc pagefile_contents space\n");

        return;
    }

    g_pagefile_state = malloc(g_pagefile_blocks * sizeof(UCHAR));

    if (g_pagefile_state == NULL) {

        printf("Could not malloc pagefile_state space\n");

        return;

    }

    for (int i = 0; i < g_pagefile_blocks; i ++) {

        g_pagefile_state[i] = DISK_BLOCK_FREE;

    }

    g_pagefile_addresses = malloc(g_pagefile_blocks * sizeof(PULONG_PTR));

    if (g_pagefile_addresses == NULL) {

        printf("Could not malloc pagefile_addresses space\n");

        return;

    }

}

VOID initialize_lists(VOID) 
{
    g_standby_list.flink = &g_standby_list;
    g_standby_list.blink = &g_standby_list;

    g_modified_list.flink = &g_modified_list;
    g_modified_list.blink = &g_modified_list;

    g_free_list.flink = &g_free_list;
    g_free_list.blink = &g_free_list;

    g_zero_list.flink = &g_zero_list;
    g_zero_list.blink = &g_zero_list;

    for (int i = 0; i < g_physical_page_count; i ++) {

        page_t* new_page = page_create(g_pfn_base, g_physical_page_numbers[i]);
        list_insert(&g_zero_list, new_page);
        new_page->list_type = ZERO;

    }

}


VOID initialize_system(VOID)
{
    BOOL privilege;

    privilege = GetPrivilege ();

    if (privilege == FALSE) {
        printf ("full_virtual_memory_test : could not get privilege\n");
        return;
    }

    #if CIRCULAR_LOG
    log_idx = 0;
    #endif    

    initialize_pages();

    initialize_user_va_space();

    initialize_pte_metadata();

    initialize_pfn_metadata();

    initialize_events();

    initialize_mod_va_space();

    initialize_lists();

    initialize_threads();
}
