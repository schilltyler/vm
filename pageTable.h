#include <stdio.h>
#include <windows.h>

#ifndef PTE_t
#define PTE_t

// physical + va linked and this page is currently being used
typedef struct {
    ULONG64 valid:1; // 1 bit (0 or 1) to determine this
    // this is physical address because as you iterate through the virtual page list,
    // you set the PTE for each va. So when you go through the list again and say I want
    // va[1], then you will get the corresponding physical frame that you put in the PTE
    ULONG64 frame_number:40;
    ULONG64 age:4; // ages up to 16;
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

extern PPTE pte_from_va(PULONG64 va);
extern PULONG_PTR va_from_pte(PTE* pte);
#endif

#ifndef PAGE_TABLE_t
#define PAGE_TABLE_t

typedef struct {
    PTE pte_list; // list of PTE's
    ULONG64 num_virtual_pages; // need this for iterating pte list
} PAGE_TABLE;

#endif
