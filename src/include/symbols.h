#pragma once


#define _HEAP_START     ((unsigned long) &_heap_start)
#define _HEAP_END       ((unsigned long) &_heap_end)
#define _STACK_START     ((unsigned long) &_stack_start)
#define _STACK_END       ((unsigned long) &_stack_end)
#define _BSS_START     ((unsigned long) &_bss_start)
#define _BSS_END       ((unsigned long) &_bss_end)
#define _DATA_START     ((unsigned long) &_data_start)
#define _DATA_END       ((unsigned long) &_data_end)
#define _TEXT_START     ((unsigned long) &_text_start)
#define _TEXT_END       ((unsigned long) &_text_end)
#define _RODATA_START     ((unsigned long) &_rodata_start)
#define _RODATA_END       ((unsigned long) &_rodata_end)
#define _MEMORY_START     ((unsigned long) &_memory_start)
#define _MEMORY_END       ((unsigned long) &_memory_end)


extern void *_heap_start;
extern void *_heap_end;
extern void *_stack_start;
extern void *_stack_end;
extern void *_bss_start;
extern void *_bss_end;
extern void *_data_start;
extern void *_data_end;
extern void *_text_start;
extern void *_text_end;
extern void *_rodata_start;
extern void *_rodata_end;
extern void *_memory_start;
extern void *_memory_end;
