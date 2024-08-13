#ifndef VM_DEBUG_H
#define VM_DEBUG_H

#define DEBUG_PTE_LOCK 0
#define DEBUG_PAGE 1
#define THREAD_STATS 0
#define SUPPORT_MULTIPLE_VA_TO_SAME_PAGE 1
#define CIRCULAR_LOG 0

#if CIRCULAR_LOG
typedef struct {

    PVOID stack_trace[8];
    //ULONG64 pfn;
    /**
     * TS:
     * Not sure how to use pte struct in an include file
     * when it itself is in an include
     * Was getting compiler error so took it out for time being
     */
    //PTE* linked_pte;
    ULONG64 thread_id;

} PAGE_LOG;
#endif

typedef struct {

    PULONG_PTR virtual_address;

} PAGEFILE_DEBUG;

#endif