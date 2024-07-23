#include <stdio.h>
#include <Windows.h>
#include "../Include/initialize.h"
#include "../Include/trim.h"


#pragma comment(lib, "advapi32.lib")

// TS: probably should return int so that we can check and make sure each initialization works

// Global lists
listhead_t g_free_list;
listhead_t g_standby_list;
listhead_t g_modified_list;

// Global pte variables
ULONG_PTR g_num_ptes;
PTE* g_pte_base;
PAGE_TABLE* g_pagetable;

// Global PA variables
ULONG_PTR g_physical_page_count;
PULONG_PTR g_physical_page_numbers;
page_t* g_pfn_base;

// Global VA variables
ULONG_PTR g_virtual_address_size;
ULONG_PTR g_virtual_address_size_in_unsigned_chunks;
PULONG_PTR g_vmem_base;

// Global disk-write variables
ULONG_PTR g_pagefile_blocks;
UCHAR* g_pagefile_contents;
UCHAR* g_pagefile_state;
LPVOID g_mod_page_va;

// Global faulting variables
int g_num_fault_threads;
int g_va_iterate_type;
int g_num_faults;

// Global Events/Threads
HANDLE g_trim_event;
HANDLE g_disk_write_event;
HANDLE g_fault_event;
HANDLE* g_threads;
VOID fault_thread();

// Global Locks
CRITICAL_SECTION g_mod_lock;
CRITICAL_SECTION g_standby_lock;
CRITICAL_SECTION g_free_lock;



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

VOID initialize_events(VOID) 
{
    g_trim_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_fault_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_disk_write_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    InitializeCriticalSectionAndSpinCount(&g_mod_lock, 16000000);
    InitializeCriticalSectionAndSpinCount(&g_standby_lock, 16000000);
    InitializeCriticalSectionAndSpinCount(&g_free_lock, 16000000);
}

VOID initialize_threads(VOID)
{
    g_threads = (HANDLE*) malloc(sizeof(HANDLE) * (2 + g_num_fault_threads));
    g_threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) trim_thread, NULL, 0, NULL);
    g_threads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) disk_write_thread, NULL, 0, NULL);

    for (int i = 0; i < g_num_fault_threads; i ++) {

        g_threads[i + 2] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) fault_thread, NULL, 0, NULL);

    }
}

VOID initialize_pages(VOID)
{
    HANDLE physical_page_handle;
    ULONG_PTR number_of_physical_pages;

    physical_page_handle = GetCurrentProcess ();

    //physical_page_count = NUMBER_OF_PHYSICAL_PAGES;

    number_of_physical_pages = g_physical_page_count;

    // make array for physical page numbers
    g_physical_page_numbers = malloc (g_physical_page_count * sizeof (ULONG_PTR));

    if (g_physical_page_numbers == NULL) {
        printf ("full_virtual_memory_test : could not allocate array to hold physical page numbers\n");
        return;
    }

    BOOL allocated;

    // get the physical pages
    allocated = AllocateUserPhysicalPages (physical_page_handle,
                                           &g_physical_page_count,
                                           g_physical_page_numbers);

    if (allocated == FALSE) {
        printf ("full_virtual_memory_test : could not allocate physical pages\n");
        return;
    }

    // could not get all of the physical pages asked for
    if (g_physical_page_count != number_of_physical_pages) {

        printf ("full_virtual_memory_test : allocated only %llu pages out of %llu pages requested\n",
                g_physical_page_count,
                number_of_physical_pages);
    }

    g_pagefile_blocks = ((g_virtual_address_size / PAGE_SIZE) - g_physical_page_count + 1);
}

VOID initialize_user_va_space(VOID) 
{
    // reserve user address space region
    //virtual_address_size = 64 * physical_page_count * PAGE_SIZE;

    // Round down to a PAGE_SIZE boundary
    g_virtual_address_size &= ~PAGE_SIZE;

    g_virtual_address_size_in_unsigned_chunks =
                        g_virtual_address_size / sizeof (ULONG_PTR);

    // allocate pages virtually
    g_vmem_base = VirtualAlloc (NULL,
                      g_virtual_address_size,
                      MEM_RESERVE | MEM_PHYSICAL,
                      PAGE_READWRITE);

    if (g_vmem_base == NULL) {
        printf ("full_virtual_memory_test : could not reserve memory\n");
        return;
    }
}

VOID initialize_pte_metadata(VOID) 
{
    // initialize PTE's we will use
    g_num_ptes = g_virtual_address_size / PAGE_SIZE;

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
    for (int i = 0; i < g_physical_page_count; i ++) {
        if (g_physical_page_numbers[i] > high_pfn) {
            high_pfn = g_physical_page_numbers[i];
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
    g_mod_page_va = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);

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

        g_pagefile_state[i] = FREE;

    }

}

VOID initialize_lists(VOID) 
{
    // create standby list, not populated yet though
    g_standby_list.flink = &g_standby_list;
    g_standby_list.blink = &g_standby_list;

    // create modified list, not populated yet though
    g_modified_list.flink = &g_modified_list;
    g_modified_list.blink = &g_modified_list;

    // create free list, then populate it with physical frame numbers
    g_free_list.flink = &g_free_list;
    g_free_list.blink = &g_free_list;

    for (int i = 0; i < g_physical_page_count; i ++) {

        page_t* new_page = page_create(g_pfn_base, g_physical_page_numbers[i]);
        list_insert(&g_free_list, new_page);

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

    initialize_pages();

    initialize_user_va_space();

    initialize_pte_metadata();

    initialize_pfn_metadata();

    initialize_events();

    initialize_mod_va_space();

    initialize_lists();

    initialize_threads();
}

#if 0
void DiskRead() {

    memcpy();

    for (int i = 0; i < 1000000; i ++) {
    
        continue;
    
    }

}

void DiskWrite() {

    memcpy();

    for (int i = 0; i < 1000000; i ++) {
    
        // TS: how to spin?
    
    }

}

// TS: not sure how to do critical section initializers

void make_critical_section() {

    SetCriticalSectionSpinCount();

}

#endif

