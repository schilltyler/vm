#include <stdio.h>
#include <Windows.h>
#include "../Include/initialize.h"
#include "../Include/trim.h"


#pragma comment(lib, "advapi32.lib")

//Global variables
PTE* pte_base;
PULONG_PTR vmem_base;
page_t* pfn_base;
listhead_t free_list;
listhead_t standby_list;
listhead_t modified_list;
ULONG_PTR num_ptes;
ULONG_PTR physical_page_count;
unsigned i;
PULONG_PTR arbitrary_va;
unsigned random_number;
BOOL allocated;
BOOL page_faulted;
BOOL privilege;
BOOL obtained_pages;
PULONG_PTR physical_page_numbers;
HANDLE physical_page_handle;
ULONG_PTR virtual_address_size;
ULONG_PTR virtual_address_size_in_unsigned_chunks;

// Global synchronization
HANDLE trim_event;
HANDLE* threads;

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
}

VOID initialize_threads(VOID)
{
    threads = (HANDLE*) malloc(sizeof(HANDLE) * 1);
    threads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) trim_thread, NULL, 0, NULL);
}

VOID initialize_pages(VOID)
{
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
}

VOID initialize_user_va_space(VOID) 
{
    // reserve user address space region
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


VOID initialize_system(VOID)
{
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

    initialize_threads();
}