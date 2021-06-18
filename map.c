#include <stdlib.h>
#include "map.h"

int hash_int(int key) {
    key = ~key + (key << 15);
    key = key ^ (key >> 12);
    key = key + (key << 2);
    key = key ^ (key >> 4);
    key = key * 2057;
    key = key ^ (key >> 16);
    return key;
}

int hash(int x, int y, int z) {
    x = hash_int(x);
    y = hash_int(y);
    z = hash_int(z);
    return x ^ y ^ z;
}

void map_alloc(Map *map) {
    map->mask = 0xfff;
    map->size = 0;
    map->data = (Entry *)calloc(map->mask + 1, sizeof(Entry)); // hashmap initialized
}

void map_free(Map *map) {
    free(map->data);
}

void map_grow(Map *map);

// map_set
//
// fills in the block, including the texture. saves it to the map
//
// @var Map *map
// @var int x : position of block
// @var int y : position of block
// @var int z : position of block
// @var int block_texture : texture of block
void map_set(Map *map, int x, int y, int z, int block_texture) {
    unsigned int index = hash(x, y, z) & map->mask; // hint: what the fuck is this?
    Entry *entry = map->data + index; // map->data = (Entry *)calloc(map->mask + 1, sizeof(Entry));
    int overwrite = 0;

    // whatis: see if the hash exists in the map. if it does,
    //  continue on until we find an entry that is empty
    while (!EMPTY_ENTRY(entry)) {
        if (entry->x == x && entry->y == y && entry->z == z) {
            overwrite = 1;
            break;
        }
        index = (index + 1) & map->mask;
        entry = map->data + index;
    }
    if (overwrite) {
        entry->w = block_texture;
    }
    else if (block_texture) {
        entry->x = x;
        entry->y = y;
        entry->z = z;
        entry->w = block_texture;
        map->size++;
        if (map->size * 2 > map->mask) {
            map_grow(map);
        }
    }
}

int map_get(Map *map, int x, int y, int z) {
    unsigned int index = hash(x, y, z) & map->mask;
    Entry *entry = map->data + index;
    while (!EMPTY_ENTRY(entry)) {
        if (entry->x == x && entry->y == y && entry->z == z) {
            return entry->w;
        }
        index = (index + 1) & map->mask;
        entry = map->data + index;
    }
    return 0;
}

void map_grow(Map *map) {
    Map new_map;
    new_map.mask = (map->mask << 1) | 1;
    new_map.size = 0;
    new_map.data = (Entry *)calloc(new_map.mask + 1, sizeof(Entry));
    for (unsigned int index = 0; index <= map->mask; index++) {
        Entry *entry = map->data + index;
        if (!EMPTY_ENTRY(entry)) {
            map_set(&new_map, entry->x, entry->y, entry->z, entry->w);
        }
    }
    free(map->data);
    map->mask = new_map.mask;
    map->size = new_map.size;
    map->data = new_map.data;
}
