#ifndef VM_PAGE_FAULT_H
#define VM_PAGE_FAULT_H

int handle_page_fault(PULONG_PTR virtual_address, LPVOID mod_page_va2);
page_t* get_free_zero_standby(PTE_LOCK* pte_lock, int pop_free_or_zero);

#endif