#include <stdio.h>
#include "../Include/page.h"
#include "../Include/pagetable.h"
#include "../Include/initialize.h"


int handle_page_fault(PULONG_PTR virtual_address, LPVOID mod_page_va2) {

    PTE_LOCK* pte_lock;
    PPTE pte;

    if (virtual_address == NULL) {

        DebugBreak();

    }

    /**
     * TS: game plan for getting pte crit sec region
     * va - vmem_base
     * divide that by page size
     * this is giving you the pte number (I think I can call this the pte index)
     * we can then use this number to figure out what lock section the pte is in
     * figure out lock sections because they are every "x" amount of ptes in the pagetable
     * pte_index % 32 (aka num_pte_regions) equals the region lock for that pte
     * 
     * Have a function for these types of lines
     * ULONG64 pte_index = ((ULONG_PTR)virtual_address - (ULONG_PTR) vmem_base) / PAGE_SIZE;
     */
    pte_lock = get_pte_lock(virtual_address);

    EnterCriticalSection(&pte_lock->lock);

    pte = pte_from_va(virtual_address);

    //#### VALID #####
    if (pte->memory.valid == 1) {

        LeaveCriticalSection(&pte_lock->lock);
        return SUCCESS;

    }

    //#### STANDBY/MODIFIED RESCUE ####
    else if (pte->transition.rescuable == 1) {

        return rescue_page(pte, virtual_address, pte_lock);

    }

    // #### BRAND NEW #####
    else if (pte->entire_format == 0) {

        return map_new_va(pte, virtual_address, pte_lock);
    
    }

    //#### ON DISK ####
    else {

        return read_disk(pte, virtual_address, pte_lock, mod_page_va2);

    }

}

int rescue_page(PPTE pte, PULONG_PTR virtual_address, PTE_LOCK* pte_lock) {

    page_t* free_page;
    boolean got_page;
    ULONG64 pfn;
    ULONG64 disk_addr;

    got_page = FALSE;

    pfn = pte->transition.frame_number;

    free_page = page_from_pfn(pfn, g_pfn_base);

    if (free_page->list_type == MODIFIED) {

        EnterCriticalSection(&g_mod_lock);
        
        if (free_page->list_type == MODIFIED) {

            if (free_page->pte != pte) {
                
                DebugBreak();

            }

            if (free_page->write_in_progress == TRUE) {

                free_page->was_rescued = TRUE;

            }
            
            else {

                list_unlink(&g_modified_list, pfn);

            }

            got_page = TRUE;

        }

        LeaveCriticalSection(&g_mod_lock);

    }

    if (free_page->list_type == STANDBY) {

        EnterCriticalSection(&g_standby_lock);

        if (free_page->list_type == STANDBY) {

            if (free_page->pte != pte) {
                
                DebugBreak();

            }

            list_unlink(&g_standby_list, pfn);

            got_page = TRUE;

        }
        
        LeaveCriticalSection(&g_standby_lock);

    }

    /**
     * TS:
     * Could we leave the pte lock above when trying to 
     * acquire the standby/modified lock?
     * We could just add in another check once we get the
     * lock to make sure the pte did not change
     * Not sure this would actually help anything though,
     * because you would just give the lock to another process
     * potentially, and we would have to wait for that to finish,
     * slowing us down
     * However, it could be beneficial if we had a situation where
     * no other process was using this particular pte lock,
     * and we let another process run while we were waiting anyway
     * for a modified/standby lock
     */

    if (got_page == FALSE) {

        DebugBreak();

    }

    if (free_page->pte != pte) {

        DebugBreak();

    }

    if (MapUserPhysicalPages(virtual_address, 1, &pfn) == FALSE) {

        printf("Could not remap rescue\n");

        DebugBreak();
                
        LeaveCriticalSection(&pte_lock->lock);

        return ERROR;

    }
            
    pte->memory.valid = 1;
    
    pte->transition.rescuable = 0;
    free_page->list_type = ACTIVE;

    disk_addr = free_page->disk_address;

    LeaveCriticalSection(&pte_lock->lock); 

    EnterCriticalSection(&g_mod_lock);
    g_pagefile_state[disk_addr] = FREE;
    LeaveCriticalSection(&g_mod_lock);

    //SetEvent(disk_write_event);

    return SUCCESS;

}

int map_new_va(PPTE pte, PULONG_PTR virtual_address, PTE_LOCK* pte_lock) {

    page_t* free_page;
    ULONG64 pfn;

    free_page = get_free_or_standby(pte_lock);

    if (free_page == NULL) {

        return ERROR;

    }

    pfn = pfn_from_page(free_page, g_pfn_base);

    if (MapUserPhysicalPages (virtual_address, 1, &pfn) == FALSE) {

        printf ("full_virtual_memory_test : could not map VA %p to page %llX\n", virtual_address, pfn);

        DebugBreak();

        LeaveCriticalSection(&pte_lock->lock);

        return ERROR;
    }

    free_page->list_type = ACTIVE;

    /**
     * Enter back into pte lock
     */
    pte->memory.valid = 1;
    pte->memory.frame_number = pfn;

    /**
     * Need this for when I trim page from something like standby and 
     * want to cut off old pte to replace with new one
     */ 
    free_page->pte = pte;

    LeaveCriticalSection(&pte_lock->lock);

    
    if (g_free_list.list_size + g_standby_list.list_size < g_physical_page_count / 4) {
        SetEvent(g_trim_event);
    }
    

    return SUCCESS;
}

int read_disk(PPTE pte, PULONG_PTR virtual_address, PTE_LOCK* pte_lock, LPVOID mod_page_va2, ULONG64 pte_region) {

    page_t* free_page;
    ULONG64 pfn;
    void* src;
    PTE new_contents;
    ULONG64 disk_addr;

    #if 0
    PTE* old_pte = pte;
    #endif

    free_page = get_free_or_standby(pte_lock);

    #if 0
    EnterCriticalSection(pte_lock->lock);

    if (pte == old_pte) {
    
        // continue business as usual
    
    }

    else {
    
        // put page back on free or standby
        // return error on the fault and retry

    }
    #endif

    if (free_page == NULL) {

        return ERROR;

    }

    if (mod_page_va2 == NULL) {

        printf("Could not allocate mod va 2\n");

        DebugBreak();

        return ERROR;

    }

    pfn = pfn_from_page(free_page, g_pfn_base);

    if (MapUserPhysicalPages(mod_page_va2, 1, &pfn) == FALSE) {

        printf("Could not map mod va 2\n");

        LeaveCriticalSection(&pte_lock->lock);

        DebugBreak();

    }

    /**
     * We may need the pte lock here to look at this
     * disk address
     */
    src = &g_pagefile_contents[pte->disk.disk_address * PAGE_SIZE];

    memcpy(mod_page_va2, src, PAGE_SIZE);

    if (MapUserPhysicalPages(mod_page_va2, 1, NULL) == FALSE) {

        printf("Could not unmap mod va 2\n");

        LeaveCriticalSection(&pte_lock->lock);

        DebugBreak();

    }

    disk_addr = pte->disk.disk_address;

    new_contents.entire_format = 0;

    new_contents.memory.valid = 1;
    new_contents.memory.frame_number = pfn;
    
    *pte = new_contents;

    free_page->pte = pte;

    /**
     * Could release pte lock before this so we don't wait have
     * to pay that cost
     */

    if (MapUserPhysicalPages(virtual_address, 1, &pfn) == FALSE) {

        printf("Could not map disk VA to page\n");

        LeaveCriticalSection(&pte_lock->lock);

        DebugBreak();

    }

    free_page->list_type = ACTIVE;

    LeaveCriticalSection(&pte_lock->lock);

    EnterCriticalSection(&g_mod_lock);
    g_pagefile_state[disk_addr] = FREE;
    LeaveCriticalSection(&g_mod_lock);

    //SetEvent(disk_write_event);

    return SUCCESS;

}


page_t* get_free_or_standby(PTE_LOCK* pte_lock) {

    /**
     * TS:
     * - Have a lock C to get rid of ABBA
     * - lock covers page_t structure of page you are trying to rescue/repurpose
     * from modified/standby
     * - can peek at head of standby/modified 
     */

    page_t* free_page;
    PULONG_PTR old_va;
    PTE_LOCK* old_pte_lock;

    EnterCriticalSection(&g_free_lock);
    free_page = list_pop(&g_free_list);
    LeaveCriticalSection(&g_free_lock);

    if (free_page == NULL) {
        
        EnterCriticalSection(&g_standby_lock);
        free_page = list_pop(&g_standby_list);

        if (free_page == NULL) {

            LeaveCriticalSection(&g_standby_lock);
            
            LeaveCriticalSection(&pte_lock->lock);

            SetEvent(g_trim_event);
    
            WaitForSingleObject(g_fault_event, INFINITE);

            return NULL;

        }

        /**
         * TS:
         * We could leave the pte lock at the top of this function,
         * and then reenter once we have the free/standby lock.
         * We would just have to add a check after we acquire
         * the lock seeing if the pte is still in the same state
         * that we think it is
         * We don't need to reacquire the pte lock at all in this function
         * so all of this code seeing if the old pte lock matched the current one
         * will be done away with because we won't have that instance anymore
         * We can do our check to see if the pte is still the same after we return
         * for this function
         * Actually no, at the bottom of this function we should
         */
        
        old_va = va_from_pte(free_page->pte);
        old_pte_lock = get_pte_lock(old_va);

        if (old_pte_lock->region == pte_lock->region) {

            free_page->pte->disk.disk_address = free_page->disk_address;
            free_page->pte->disk.always_zero2 = 0;

        }

        else {

            if (TryEnterCriticalSection(&old_pte_lock->lock) == 0) {

                list_insert(&g_standby_list, free_page);

                LeaveCriticalSection(&pte_lock->lock);
                LeaveCriticalSection(&g_standby_lock);

                return NULL;
            
            }

            free_page->pte->disk.disk_address = free_page->disk_address;
            free_page->pte->disk.always_zero2 = 0;

        
            LeaveCriticalSection(&old_pte_lock->lock);

        }

        LeaveCriticalSection(&g_standby_lock);
    }

    return free_page;

}