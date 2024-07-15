#include <stdio.h>
#include <Windows.h>
#include "../Include/initialize.h"
#include "../Include/trim.h"


#pragma comment(lib, "advapi32.lib")

// TS: probably should return int so that we can check and make sure each initialization works

// Global lists
listhead_t free_list;
listhead_t standby_list;
listhead_t modified_list;

// Global pte variables
ULONG_PTR num_ptes;
PTE* pte_base;

// Global PA variables
ULONG_PTR physical_page_count;
ULONG_PTR number_of_physical_pages;
PULONG_PTR physical_page_numbers;
page_t* pfn_base;

// Global VA variables
ULONG_PTR virtual_address_size;
ULONG_PTR virtual_address_size_in_unsigned_chunks;
PULONG_PTR vmem_base;

// Global disk-write variables
ULONG_PTR pagefile_blocks;
UCHAR* pagefile_contents;
UCHAR* pagefile_state;
LPVOID mod_page_va;
LPVOID mod_page_va2;

// Global faulting variables
int num_fault_threads;
int va_iterate_type;
int num_faults;

// Global Events/Threads
HANDLE trim_event;
HANDLE disk_write_event;
HANDLE fault_event;
HANDLE* threads;
VOID fault_thread();

// Global Locks
CRITICAL_SECTION pte_lock;
CRITICAL_SECTION mod_lock;
CRITICAL_SECTION standby_lock;



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
    trim_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    fault_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    disk_write_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    InitializeCriticalSection(&mod_lock);
    InitializeCriticalSection(&standby_lock);
    //InitializeCriticalSectionAndSpinCount(&mod_lock, 16000000);
    //InitializeCriticalSectionAndSpinCount(&standby_lock, 16000000);
}

VOID initialize_threads(VOID)
{
    threads = (HANDLE*) malloc(sizeof(HANDLE) * (2 + num_fault_threads));
    threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) trim_thread, NULL, 0, NULL);
    threads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) disk_write_thread, NULL, 0, NULL);

    for (int i = 0; i < num_fault_threads; i ++) {

        threads[i + 2] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) fault_thread, NULL, 0, NULL);

    }
}

VOID initialize_pages(VOID)
{
    HANDLE physical_page_handle;

    physical_page_handle = GetCurrentProcess ();

    //physical_page_count = NUMBER_OF_PHYSICAL_PAGES;

    // make array for physical page numbers
    physical_page_numbers = malloc (physical_page_count * sizeof (ULONG_PTR));

    if (physical_page_numbers == NULL) {
        printf ("full_virtual_memory_test : could not allocate array to hold physical page numbers\n");
        return;
    }

    BOOL allocated;

    // get the physical pages
    allocated = AllocateUserPhysicalPages (physical_page_handle,
                                           &physical_page_count,
                                           physical_page_numbers);

    if (allocated == FALSE) {
        printf ("full_virtual_memory_test : could not allocate physical pages\n");
        return;
    }

    // could not get all of the physical pages asked for
    if (physical_page_count != number_of_physical_pages) {

        printf ("full_virtual_memory_test : allocated only %llu pages out of %llu pages requested\n",
                physical_page_count,
                number_of_physical_pages);
    }
}

VOID initialize_user_va_space(VOID) 
{
    // reserve user address space region
    //virtual_address_size = 64 * physical_page_count * PAGE_SIZE;

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
}

VOID initialize_pte_metadata(VOID) 
{
    // initialize PTE's we will use
    num_ptes = virtual_address_size / PAGE_SIZE;

    ULONG_PTR num_pte_bytes = num_ptes * sizeof(PTE);

    pte_base = malloc(num_pte_bytes);

    if (pte_base == NULL){
        printf("Could not malloc pte_base\n");
        return;
    }

    memset(pte_base, 0, num_pte_bytes);

    InitializeCriticalSection(&pte_lock);
    //InitializeCriticalSectionAndSpinCount(&pte_lock, 16000000);
}

VOID initialize_pfn_metadata(VOID)
{
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
}

VOID initialize_mod_va_space(VOID) 
{
    mod_page_va = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);

    if (mod_page_va == NULL) {
    
        printf("Could not allocate mod page va\n");

        return;

    }

    mod_page_va2 = VirtualAlloc(NULL, PAGE_SIZE, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);

    if (mod_page_va2 == NULL) {

        printf("Could not allocate mod page va2\n");

        return;

    }

    pagefile_contents = malloc(pagefile_blocks * PAGE_SIZE);

    if (pagefile_contents == NULL) {
        
        printf("Could not malloc pagefile_contents space\n");

        return;
    }

    pagefile_state = malloc(pagefile_blocks * PAGE_SIZE);

    if (pagefile_state == NULL) {

        printf("Could not malloc pagefile_state space\n");

        return;

    }

}

VOID initialize_lists(VOID) 
{
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

