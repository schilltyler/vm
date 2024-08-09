#ifndef VM_INIT_H
#define VM_INIT_H

#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/pagefault.h"
#include "../Include/debug.h"


// Global definitions
#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)
#define GB(x) ((x) * 1024 * 1024 * 1024)
#define PAGE_SIZE 4096
#define LOG_SIZE 512
#define SUCCESS 1
#define ERROR 0
#define FREE 0
#define MODIFIED 0
#define STANDBY 1
#define ACTIVE 2
#define ACCESS_AMOUNT MB(10)
#define VIRTUAL_ADDRESS_SIZE MB(16)
#define NUMBER_OF_PHYSICAL_PAGES ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / 2)
#define VA_ITERATE_TYPE 1
#define NUM_PTE_REGIONS 128
#define NUM_PTES_PER_REGION ((VIRTUAL_ADDRESS_SIZE / PAGE_SIZE) / NUM_PTE_REGIONS)


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
extern MEM_EXTENDED_PARAMETER g_vmem_parameter;

// Global disk-write variables
extern ULONG_PTR g_pagefile_blocks;
extern UCHAR* g_pagefile_contents;
extern UCHAR* g_pagefile_state;
extern LPVOID g_mod_page_va;

// Global faulting variables
extern int g_num_fault_threads;
extern int g_va_iterate_type;

// Global Events/Threads
extern HANDLE g_trim_event;
extern HANDLE g_disk_write_event;
extern HANDLE g_fault_event;
extern HANDLE g_kill_trim_event;
extern HANDLE g_trim_finished_event;
extern HANDLE* g_trim_handles;
extern HANDLE* g_threads;
extern VOID fault_thread();

// Global Locks
extern CRITICAL_SECTION g_mod_lock;
extern CRITICAL_SECTION g_standby_lock;
extern CRITICAL_SECTION g_free_lock;

// Global Functions
extern VOID initialize_system(VOID);

// Thread Stats
extern ULONG64 g_num_faults;

// Debugging Variables
#if CIRCULAR_LOG
extern PAGE_LOG g_page_log[LOG_SIZE];
extern volatile ULONG64 log_idx;
#endif

#endif