#ifndef VM_PTE_H
#define VM_PTE_H

#include <stdio.h>
#include <windows.h>
#include "../Include/debug.h"

typedef struct {

    ULONG64 valid:1;
    ULONG64 frame_number:40;
    ULONG64 age:4;

} valid_pte;


typedef struct {

    ULONG64 always_zero:1;
    ULONG64 disk_address:40;
    ULONG64 always_zero2:1;

} disk_pte;

typedef struct {

    ULONG64 always_zero:1;
    ULONG64 frame_number:40;
    ULONG64 rescuable:1;
    //ULONG64 list:1; // differentiate between modified and standby. ** Get from page_t**

} transition_pte;


typedef struct {

    /**
     * Use a union here because PTE could have multiple states 
     * (don't need bits for all of them)
     */
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


PAGE_TABLE* create_pagetable();
PPTE pte_from_va(PULONG64 va);
PULONG_PTR va_from_pte(PTE* pte);
PTE_LOCK* get_pte_lock(PULONG_PTR virtual_address);
/**
 * TS:
 * Look into adding the other two functions here
 * Getting a ton of compiler errors when I try to put them in
 */

#endif
