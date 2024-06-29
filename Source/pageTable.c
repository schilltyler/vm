#include <stdio.h>
#include <windows.h>
#include "../Include/pagetable.h"
#include "../Include/page.h"
#include "../Include/initialize.h"



PPTE pte_from_va(PULONG64 va) { 

    // find how many spaces are between our va and the first va in the allocated chunk
    ULONG_PTR difference = (ULONG_PTR) va - (ULONG_PTR) vmem_base;
    
    // divide this space by the page size to know how many pages the space is (each page has a pte)
    difference /= PAGE_SIZE;

    // starting from the first pte, the difference tells you how far from this your pte is
    return pte_base + difference;
}


// TS: make va's PVOID so that compiler doesn't do anything do it
PULONG_PTR va_from_pte(PTE* pte) { 

    ULONG_PTR difference = (ULONG_PTR) (pte - pte_base);

    difference *= PAGE_SIZE;

    PULONG_PTR va = vmem_base + (difference / sizeof(ULONG_PTR));

    return va;
}


page_t* page_from_pfn(ULONG64 pfn, page_t* pfn_base) {

    return pfn_base + pfn;

}

ULONG64 pfn_from_page(page_t* page, page_t* pfn_base) {
     
    return page - pfn_base;
}
