#ifndef VM_INIT_H
#define VM_INIT_H

#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/pagefault.h"

// Global definitions
#define PAGE_SIZE                   4096
#define MB(x)                       ((x) * 1024 * 1024)
#define VIRTUAL_ADDRESS_SIZE        MB(16)
#define VIRTUAL_ADDRESS_SIZE_IN_UNSIGNED_CHUNKS        (VIRTUAL_ADDRESS_SIZE / sizeof (ULONG_PTR))
#define NUMBER_OF_PHYSICAL_PAGES   ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / 64) // ~1% of virtual address space
#define PAGEFILE_BLOCKS ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) - NUMBER_OF_PHYSICAL_PAGES + 1)

#define SUCCESS 1
#define ERROR 0
#define FREE 0
#define MODIFIED 0
#define STANDBY 1


// Global variables
extern PTE* pte_base;
extern PULONG_PTR vmem_base;
extern page_t* pfn_base;
extern listhead_t free_list;
extern listhead_t standby_list;
extern listhead_t modified_list;
extern ULONG_PTR num_ptes;
extern ULONG_PTR physical_page_count;
extern unsigned i;
extern PULONG_PTR arbitrary_va;
extern unsigned random_number;
extern BOOL allocated;
extern BOOL page_faulted;
extern BOOL privilege;
extern BOOL obtained_pages;
extern PULONG_PTR physical_page_numbers;
extern HANDLE physical_page_handle;
extern ULONG_PTR virtual_address_size;
extern ULONG_PTR virtual_address_size_in_unsigned_chunks;
extern UCHAR pagefile_contents[PAGEFILE_BLOCKS * PAGE_SIZE];
extern UCHAR pagefile_state[PAGEFILE_BLOCKS];
extern LPVOID mod_page_va;
extern LPVOID mod_page_va2;


// Global Synchronization
extern HANDLE trim_event;
extern HANDLE disk_write_event;
extern HANDLE fault_event;

//extern HANDLE fault_event;
extern HANDLE* threads;
extern CRITICAL_SECTION pte_lock;
extern CRITICAL_SECTION mod_lock;
extern CRITICAL_SECTION standby_lock;


// Functions
extern VOID initialize_system(VOID);

#endif