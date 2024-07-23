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
#define ACTIVE 2

/**
 * TS:
 * Go through all global variables and add a "g_" or
 * something before the name of each one so that
 * I always know scope of particular variable
 */

// Global lists
extern listhead_t g_free_list;
extern listhead_t g_standby_list;
extern listhead_t g_modified_list;

// Global pte variables
extern ULONG_PTR g_num_ptes;
extern PTE* g_pte_base;
extern PAGE_TABLE* g_pagetable;

// Global PA variables
extern ULONG_PTR g_physical_page_count;
extern PULONG_PTR g_physical_page_numbers;
extern page_t* g_pfn_base;

// Global VA variables
extern ULONG_PTR g_virtual_address_size;
extern ULONG_PTR g_virtual_address_size_in_unsigned_chunks;
extern PULONG_PTR g_vmem_base;

// Global disk-write variables
extern ULONG_PTR g_pagefile_blocks;
extern UCHAR* g_pagefile_contents;
extern UCHAR* g_pagefile_state;
extern LPVOID g_mod_page_va;

// Global faulting variables
extern int g_num_fault_threads;
extern int g_va_iterate_type;
extern int g_num_faults;

// Global Events/Threads
extern HANDLE g_trim_event;
extern HANDLE g_disk_write_event;
extern HANDLE g_fault_event;
extern HANDLE* g_threads;
extern VOID fault_thread();

// Global Locks
extern CRITICAL_SECTION g_mod_lock;
extern CRITICAL_SECTION g_standby_lock;
extern CRITICAL_SECTION g_free_lock;


// Global Functions
extern VOID initialize_system(VOID);

#endif