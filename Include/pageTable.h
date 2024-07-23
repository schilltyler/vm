#ifndef VM_PTE_H
#define VM_PTE_H

#include <stdio.h>
#include <windows.h>
#include "../Include/debug.h"



// physical + va linked and this page is currently being used
typedef struct {
    ULONG64 valid:1;
    ULONG64 frame_number:40;
    ULONG64 age:4;
} valid_pte;

// these pages are ready to be used (pa + va not linked)
typedef struct {
    ULONG64 always_zero:1;
    ULONG64 disk_address:40; // tells where to find data on disk
    ULONG64 always_zero2:1; // this is at same location as rescue
} disk_pte;

typedef struct {
    ULONG64 always_zero:1;
    ULONG64 frame_number:40;
    ULONG64 rescuable:1;
    //ULONG64 list:1; // differentiate between modified and standby. ** Get from page_t**
} transition_pte;

// PTE could have multiple states (don't need bits for all of them)
typedef struct {
    union {
        valid_pte memory;
        disk_pte disk;
        transition_pte transition;
        ULONG64 entire_format;
    };
} PTE, *PPTE;

typedef struct {

    CRITICAL_SECTION lock;
    ULONG64 region;

    #if DEBUG_PTE_LOCK

    ULONG64 owning_thread;

    // use getcurrentthreadid() with this

    #endif

} PTE_LOCK;

typedef struct {

    PTE_LOCK* pte_lock_sections;

} PAGE_TABLE;


// Function prototypes
PPTE pte_from_va(PULONG64 va);
PULONG_PTR va_from_pte(PTE* pte);
PAGE_TABLE* create_pagetable();
PTE_LOCK* get_pte_lock(PULONG_PTR virtual_address);
//TS: look into adding the other two functions here

#endif
