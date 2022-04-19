#pragma once


#include <stdint.h>
#include <stdbool.h>


typedef struct MapNode {
    uint64_t key;
    void* value;
    struct MapNode* left;
    struct MapNode* right;
} MapNode;

typedef struct Map {
    MapNode* head;
} Map;


Map* map_new();
void map_free(Map* map);
void* map_get(Map* map, uint64_t key);
bool map_insert(Map* map, uint64_t key, void* value);

void map_print(Map* map);
