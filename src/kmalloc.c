#include <kmalloc.h>
#include <mmu.h>
#include <page_alloc.h>
#include <printf.h>


typedef struct Allocation {
    size_t size;
    struct Allocation* prev;
    struct Allocation* next;
} Allocation;


Allocation* free_head;
uint64_t kernel_heap_vaddr = KERNEL_HEAP_START_VADDR;


void free_list_push(Allocation* node) {
    node->next = free_head;
    node->prev = free_head->prev;

    node->prev->next = node;
    node->next->prev = node;
}

void free_list_remove(Allocation* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

int64_t split_node(Allocation* node, size_t bytes) {
    Allocation* new_node;
    int64_t new_size;

    new_size = node->size - (sizeof(Allocation) + bytes);
    if (new_size <= 0) {
        return new_size;
    }

    new_node = (Allocation*) (((uint8_t*) node) + sizeof(Allocation) + bytes);
    new_node->size = new_size;
    node->size = bytes;

    new_node->next = node->next;
    new_node->prev = node;
    new_node->next->prev = new_node;
    node->next = new_node;

    return new_size;
}

bool kmalloc_init(void) {
    Allocation* page;

    page = page_zalloc(1);
    if (!mmu_map(kernel_mmu_table, kernel_heap_vaddr, (uint64_t) page, PB_READ | PB_WRITE)) {
        return false;
    }

    free_head = page; // todo: change to virt
    free_head->size = 0;
    free_head->prev = free_head;
    free_head->next = free_head;

    return true;
}

void* kmalloc(size_t bytes) {
    Allocation* free_node;
    int num_pages;

    for (free_node = free_head->next; free_node != free_head; free_node = free_node->next) {
        if (free_node->size >= bytes) {
            break;
        }
    }

    if (free_node == free_head) {
        // No node big enough for bytes. Alloc more
        num_pages = (sizeof(Allocation) + bytes + PS_4K - 1) / PS_4K;
        
        free_node = page_alloc(num_pages);
        if (free_node == NULL) {
            return NULL;
        }
        
        // todo: map to virt
        // free_node = virt
        free_node->size = ((uint64_t) num_pages) * PS_4K - sizeof(Allocation);
        free_list_push(free_node);
    }

    if (split_node(free_node, bytes) < 0) {
        return NULL;
    }

    free_list_remove(free_node);

    return free_node + 1;   // Skip sizeof(Allocation) bytes
}

void* kzalloc(size_t bytes) {
    return NULL;
}

void kfree(void* mem) {
    Allocation* node;

    node = ((Allocation*) mem) - 1;
    free_list_push(node);
}

void coalesce_free_list(void) {
    printf("TODO\n");
}

void kmalloc_print() {
    Allocation* it;
    uint64_t num_nodes;
    uint64_t free_bytes;

    printf("0x%08x: { size: %5d, prev: 0x%08x, next: 0x%08x }\n", free_head, free_head->size, free_head->prev, free_head->next);

    num_nodes = 1;
    free_bytes = 0;
    for (it = free_head->next; it != free_head; it = it->next) {
        printf("0x%08x: { size: %5d, prev: 0x%08x, next: 0x%08x }\n", it, it->size, it->prev, it->next);

        num_nodes++;
        free_bytes += it->size;
    }

    printf("Total nodes: %d, Free bytes: %d\n", num_nodes, free_bytes);
}
