// Simple hash table implemented in C.
// https://benhoyt.com/writings/hash-table-in-c/
// Author Ben Hoyt

#include "ht.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// Hash table entry (slot may be filled or empty).
// Hash table structure: create with ht_create, free with ht_destroy.
struct ht {
  ht_entry* entries;  // hash slots
  size_t capacity;    // size of _entries array
  size_t length;      // number of items in hash table
};
#define INITIAL_CAPACITY 16  // must not be zero
ht* ht_create(size_t log2initalCapacity) {
  // Allocate space for hash table struct.
  ht* table=malloc(sizeof(struct ht));
  if (table==NULL) return NULL;
  table->length=0;
  table->capacity=log2initalCapacity?(1<<log2initalCapacity):INITIAL_CAPACITY;
  // Allocate (zero'd) space for entry buckets.
  table->entries=calloc(table->capacity, sizeof(ht_entry));
  if (table->entries==NULL) {
    free(table); // error, free table before we return!
    return NULL;
  }
  return table;
}
void ht_destroy(ht* table) {
  for (size_t i=0; i<table->capacity; i++) {
    free((void*)table->entries[i].key);
  }
  free(table->entries);
  free(table);
}
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL
// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const char* key) {
  uint64_t hash=FNV_OFFSET;
  for (const char* p=key; *p; p++) {
    hash ^= (uint64_t)(unsigned char)(*p);
    hash *= FNV_PRIME;
  }
  return hash;
}
void* ht_get(ht* table, const char* key) {
  // AND hash with capacity-1 to ensure it's within entries array.
  if (!table) { fprintf(stderr,"ht_get table is NULL\n");return NULL;}
  uint64_t hash=hash_key(key);
  size_t index=(size_t)(hash & (uint64_t)(table->capacity - 1));
  // Loop till we find an empty entry.
  ht_entry *ee=table->entries;
  while (ee[index].key!=NULL) {
    if (ee[index].key_hash==hash && strcmp(key,ee[index].key)==0) {
      return ee[index].value;
    }
    // Key wasn't in this slot, move to next (linear probing).
    index++;
    if (index>=table->capacity) index=0; // At end of entries array, wrap around.
  }
  return NULL;
}
// Internal function to set an entry (without expanding table).
static ht_entry* ht_set_entry(ht_entry* entries, size_t capacity,const char* key, uint64_t hash, void* value, size_t* plength) {
  // AND hash with capacity-1 to ensure it's within entries array.
  //uint64_t hash=hash_key(key);
  size_t index=(size_t)(hash & (uint64_t)(capacity - 1));
  // Loop till we find an empty entry.
  while (entries[index].key!=NULL) {
    if (entries[index].key_hash==hash && strcmp(key,entries[index].key)==0) {
      // Found key (it already exists), update value.
      entries[index].value=value;
      return entries+index;
    }
    // Key wasn't in this slot, move to next (linear probing).
    if (++index>=capacity) index=0;      // At end of entries array, wrap around.
  }
  // Didn't find key, allocate+copy if needed, then insert it.
  if (plength!=NULL) {
    key=strdup(key);
    if (key==NULL) return NULL;
    (*plength)++;
  }
  entries[index].key=(char*)key;
  entries[index].key_hash=hash;
  entries[index].value=value;
  return NULL;
}
// Expand hash table to twice its current size. Return true on success,
// false if out of memory.
static bool ht_expand(ht* table) {
  // Allocate new entries array.
  size_t new_capacity=table->capacity * 2;
  if (new_capacity<table->capacity) return false;  // overflow (capacity would be too big)
  ht_entry* new_entries=calloc(new_capacity, sizeof(ht_entry));
  if (new_entries==NULL) return false;
  // Iterate entries, move all non-empty ones to new table's entries.
  for (size_t i=0; i<table->capacity; i++) {
    ht_entry entry=table->entries[i];
    if (entry.key!=NULL){
      ht_set_entry(new_entries,new_capacity, entry.key,entry.key_hash,entry.value,NULL);
    }
  }
  // Free old entries array and update this table's details.
  free(table->entries);
  table->entries=new_entries;
  table->capacity=new_capacity;
  return true;
}
ht_entry* ht_set(ht* table, const char* key, void* value) {
  assert(value!=NULL);
  if (value==NULL) return NULL;
  // If length will exceed half of current capacity, expand it.
  if (table->length>=table->capacity / 2) {
    if (!ht_expand(table)) return NULL;
  }
  // Set entry and update length.
  return ht_set_entry(table->entries, table->capacity,key,hash_key(key), value, &table->length);
}
size_t ht_length(ht* table) {
  return table->length;
}
hti ht_iterator(ht* table) {
  hti it;
  it._table=table;
  it._index=0;
  return it;
}
bool ht_next(hti* it) {
  // Loop till we've hit end of entries array.
  ht* table=it->_table;
  while (it->_index<table->capacity) {
    size_t i=it->_index;
    it->_index++;
    if (table->entries[i].key!=NULL) {
      // Found next non-empty item, update iterator key and value.
      ht_entry entry=table->entries[i];
      it->key=entry.key;

      it->value=entry.value;
      return true;
    }
  }
  return false;
}
