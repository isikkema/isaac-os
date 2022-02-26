#include <kmalloc.h>
#include <mmu.h>
#include <page_alloc.h>
#include <printf.h>
#include <lock.h>
#include <string.h>


typedef struct Allocation {
    size_t size;
    struct Allocation* prev;
    struct Allocation* next;
} Allocation;


Allocation* free_head;
Mutex kmalloc_lock;
uint64_t kernel_heap_vaddr = KERNEL_HEAP_START_VADDR;


void free_list_insert_after(Allocation* node, Allocation* dst) {
    node->prev = dst;
    node->next = dst->next;

    dst->next->prev = node;
    dst->next = node;
}

void free_list_remove(Allocation* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

// Splits the given node into two nodes, retaining at least bytes in given node.
// Returns the size of the newly created node, 0 if not split, or -1 in case of error.
int64_t split_node(Allocation* node, size_t bytes) {
    Allocation* new_node;
    int64_t new_size;

    if (node->size < bytes) {
        return -1;  // This node isn't big enough to hold bytes anyways. Error
    } else if (node->size <= bytes + sizeof(Allocation)) {
        return 0;   // Big enough to hold bytes but not worth it to split the node.
    }

    // This node is big enough to split

    // Create new node
    new_size = node->size - (bytes + sizeof(Allocation));
    new_node = (Allocation*) (((uint8_t*) node) + sizeof(Allocation) + bytes);
    new_node->size = new_size;

    node->size = bytes;

    free_list_insert_after(new_node, node);

    return new_size;
}

bool kmalloc_init(void) {
    Allocation* page;

    page = page_zalloc(1);
    if (!mmu_map(kernel_mmu_table, kernel_heap_vaddr, (uint64_t) page, PB_READ | PB_WRITE)) {
        return false;
    }

    free_head = page; // todo: change to virt
    free_head->size = PS_4K - sizeof(Allocation);
    free_head->prev = free_head;
    free_head->next = free_head;

    if (split_node(free_head, 0) < 0) {
        return false;
    }

    return true;
}

void* kmalloc(size_t bytes) {
    Allocation* free_node;
    int num_pages;

    mutex_sbi_lock(&kmalloc_lock);

    // Find first node big enough to hold bytes
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
            mutex_unlock(&kmalloc_lock);
            return NULL;
        }
        
        // todo: map to virt
        // free_node = virt
        free_node->size = ((uint64_t) num_pages) * PS_4K - sizeof(Allocation);
        free_list_insert_after(free_node, free_head->prev);
    }

    if (split_node(free_node, bytes) < 0) {
        mutex_unlock(&kmalloc_lock);
        return NULL;
    }

    free_list_remove(free_node);

    mutex_unlock(&kmalloc_lock);
    return free_node + 1;   // Skip sizeof(Allocation) bytes
}

void* kzalloc(size_t bytes) {
    void* mem;

    mem = kmalloc(bytes);
    if (mem == NULL) {
        return NULL;
    }

    return memset(mem, 0, bytes);
}

void kfree(void* mem) {
    Allocation* node;
    Allocation* it;

    node = ((Allocation*) mem) - 1;
    
    mutex_sbi_lock(&kmalloc_lock);

    // Find where node/mem belongs in contiguous memory
    for (it = free_head->prev; it != free_head; it = it->prev) {
        if (it < node) {
            break;
        }
    }

    // `it` is now the node right before mem in contiguous memory
    free_list_insert_after(node, it);
    coalesce_free_list();

    mutex_unlock(&kmalloc_lock);
}

// This should work in virt mem
void coalesce_free_list(void) {
    Allocation* it;

    it = free_head->next;
    while (it != free_head) {
        if (((uint8_t*) it) + sizeof(Allocation) + it->size == (uint8_t*) it->next) {   // If it->next is right after it in contiguous mem, combine them
            it->size += sizeof(Allocation) + it->next->size;
            free_list_remove(it->next);
        } else {
            it = it->next;
        }
    }
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
