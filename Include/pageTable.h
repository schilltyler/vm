#ifndef VM_PTE_H
#define VM_PTE_H

#include <stdio.h>
#include <windows.h>


// physical + va linked and this page is currently being used
typedef struct {
    ULONG64 valid:1;
    ULONG64 frame_number:40;
    ULONG64 age:4;
} valid_pte;

// these pages are ready to be used (pa + va not linked)
typedef struct {
    ULONG64 always_zero:1;
    ULONG64 disc_address:40; // tells where to find data on disc
    ULONG64 on_disc:1; // tells us whether we have written data to disc yet
} invalid_pte;

// PTE could have multiple states (don't need bits for all of them)
typedef struct {
    union {
        valid_pte memory;
        invalid_pte disc;
        // TS: what does this mean exactly?
        ULONG64 entire_format;
    };
} PTE, *PPTE;

typedef struct {
    PTE pte_list; 
    ULONG64 num_virtual_pages;
} PAGE_TABLE;


// Function prototypes
PPTE pte_from_va(PULONG64 va);
PULONG_PTR va_from_pte(PTE* pte);
//TS: look into adding the other two functions here

#endif
