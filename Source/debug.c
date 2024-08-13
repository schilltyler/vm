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

#if DEBUG_PAGE

void copy_page_fields(page_t* page) {

    page->page_debug.flink = page->flink;
    page->page_debug.blink = page->blink;
    page->page_debug.disk_address = page->disk_address;
    page->page_debug.list_type = page->list_type;
    page->page_debug.pte = page->pte;
    page->page_debug.was_rescued = page->was_rescued;
    page->page_debug.write_in_progress = page->write_in_progress;
    
}

#endif