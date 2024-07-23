#include <stdio.h>
#include <windows.h>
#include "../Include/pagetable.h"
#include "../Include/page.h"
#include "../Include/initialize.h"


PAGE_TABLE* create_pagetable() {

    PAGE_TABLE* pagetable = (PAGE_TABLE*) malloc(sizeof(PAGE_TABLE));

    if (pagetable == NULL) {

        printf("Could not malloc pagetable\n");

        DebugBreak();

        return NULL;
        
    }

    PTE_LOCK* locks = (PTE_LOCK*) malloc(sizeof(PTE_LOCK) * 128);

    if (locks == NULL) {

        printf("Could not malloc list of pte locks\n");

        DebugBreak();

        return NULL;

    }

    // Going with 128 pte regions, which means 32 ptes per region
    for (int i = 0; i < 128; i ++) {

        InitializeCriticalSectionAndSpinCount(&locks[i].lock, 16000000);
        locks[i].region = i;

    }

    pagetable->pte_lock_sections = locks;

    return pagetable;

}


PPTE pte_from_va(PULONG64 va) { 

    // find how many spaces are between our va and the first va in the allocated chunk
    ULONG_PTR difference = (ULONG_PTR) va - (ULONG_PTR) g_vmem_base;
    
    // divide this space by the page size to know how many pages the space is (each page has a pte)
    difference /= PAGE_SIZE;

    // starting from the first pte, the difference tells you how far from this your pte is
    return g_pte_base + difference;
}


// TS: make va's PVOID so that compiler doesn't do anything do it
PULONG_PTR va_from_pte(PTE* pte) { 

    ULONG_PTR difference = (ULONG_PTR) (pte - g_pte_base);

    difference *= PAGE_SIZE;

    PULONG_PTR va = g_vmem_base + (difference / sizeof(ULONG_PTR));

    return va;
}


page_t* page_from_pfn(ULONG64 pfn, page_t* pfn_base) {

    return pfn_base + pfn;

}

ULONG64 pfn_from_page(page_t* page, page_t* pfn_base) {
     
    return page - pfn_base;
}

PTE_LOCK* get_pte_lock(PULONG_PTR virtual_address) {

    ULONG64 pte_index = ((ULONG_PTR)virtual_address - (ULONG_PTR) g_vmem_base) / PAGE_SIZE;
    ULONG64 pte_region = pte_index / 32;
    PTE_LOCK* pte_lock = &g_pagetable->pte_lock_sections[pte_region];

    return pte_lock;

}