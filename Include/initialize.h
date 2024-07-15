#ifndef VM_INIT_H
#define VM_INIT_H

#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/pagefault.h"

// Global definitions
#define PAGE_SIZE                   4096
#define SUCCESS 1
#define ERROR 0
#define FREE 0
#define MODIFIED 0
#define STANDBY 1

// Global lists
extern listhead_t free_list;
extern listhead_t standby_list;
extern listhead_t modified_list;

// Global pte variables
extern ULONG_PTR num_ptes;
extern PTE* pte_base;

// Global PA variables
extern ULONG_PTR physical_page_count;
extern ULONG_PTR number_of_physical_pages;
extern PULONG_PTR physical_page_numbers;
extern page_t* pfn_base;

// Global VA variables
extern ULONG_PTR virtual_address_size;
extern ULONG_PTR virtual_address_size_in_unsigned_chunks;
extern PULONG_PTR vmem_base;

// Global disk-write variables
extern ULONG_PTR pagefile_blocks;
extern UCHAR* pagefile_contents;
extern UCHAR* pagefile_state;
extern LPVOID mod_page_va;
extern LPVOID mod_page_va2;

// Global faulting variables
extern int num_fault_threads;
extern int va_iterate_type;
extern int num_faults;

// Global Events/Threads
extern HANDLE trim_event;
extern HANDLE disk_write_event;
extern HANDLE fault_event;
extern HANDLE* threads;
extern VOID fault_thread();

// Global Locks
extern CRITICAL_SECTION pte_lock;
extern CRITICAL_SECTION mod_lock;
extern CRITICAL_SECTION standby_lock;


// Global Functions
extern VOID initialize_system(VOID);

#endif