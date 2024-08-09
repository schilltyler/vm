#include <stdio.h>
#include <Windows.h>
#include "../Include/debug.h"
#include "../Include/initialize.h"
#include "../Include/pagetable.h"

#if CIRCULAR_LOG

void log_page(page_t* page) {

    ULONG64 curr_log_idx = InterlockedIncrement64(&log_idx) % LOG_SIZE;

    PAGE_LOG log;
    //log.linked_pte = page->pte;
    //log.pfn = pfn_from_page(page, g_pfn_base);
    log.thread_id = GetCurrentThreadId();
    CaptureStackBackTrace(0, 8, log.stack_trace, NULL);

    g_page_log[curr_log_idx] = log;

}

#endif