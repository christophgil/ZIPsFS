// Simple hash table implemented in C.
// https://benhoyt.com/writings/hash-table-in-c/
// Author Ben Hoyt
// Modified by Christoph Gille
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <malloc.h>

#ifndef log_error
#define log_error(...)  printf("\x1B[31m Error \x1B[0m"),fprintf(stderr,__VA_ARGS__)
#endif


#define HT_STORE_KEYS_IN_POOL (1<<31)
#define _HT_STORE_KEYS_MAX 16

typedef struct ht ht;
typedef struct {
  uint64_t key_hash;
  const char* key;  // key is NULL if this slot is empty
  void* value;
} ht_entry;
struct ht {
unsigned int flags,capacity,length;
  ht_entry* entries;  // hash slots
  char **keys[_HT_STORE_KEYS_MAX];
  int keys_d1,keys_d2,keys_d3;

};


bool _ht_addKeyToStore(ht *t,char *k,int l){
  const int DIM=2048;
  if (l>(2048>>3)) return false; /* Not big Strings */

  if (!t->keys[t->keys_d1]) t->keys[t->keys_d1]=malloc(DIM*sizeof(void*));
  if (!t->keys[t->keys_d1][t->keys_d2]) t->keys[t->keys_d1][t->keys_d2]=malloc(DIM*sizeof(void*));
  /*TODO*/
  return true;
}

ht* ht_create(unsigned int log2initalCapacity){
  ht* table=malloc(sizeof(struct ht));
  if (!table){
    log_error("ht.c: malloc table\n");
     return NULL;
  }
  table->flags=log2initalCapacity&0xFF000000;
  log2initalCapacity&=0xff;
  table->length=0;
  table->entries=calloc(table->capacity=log2initalCapacity?(1<<log2initalCapacity):16, sizeof(ht_entry));
  if (!table->entries){
    log_error("ht.c: calloc table->entries\n");
    free(table);
    return NULL;
  }
  return table;
}
void ht_destroy(ht* table){
  for (unsigned int i=0;i<table->capacity;i++)  free((void*)table->entries[i].key);
  free(table->entries);
  free(table);
}
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL
// Return 64-bit FNV-1a hash for key (NUL-terminated). See  https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const char* key){
  uint64_t hash=FNV_OFFSET;
  for (const char* p=key; *p;p++){
    hash^=(uint64_t)(unsigned char)(*p);
    hash*=FNV_PRIME;
  }
  return hash;
}
const ht_entry* ht_get_entry(ht* table, const char* key){
  // AND hash with capacity-1 to ensure it's within entries array.
  if (!table){ fprintf(stderr,"ht_get table is NULL\n");return NULL;}
  const uint64_t hash=hash_key(key);
  unsigned int i=(unsigned int)(hash & (uint64_t)(table->capacity - 1));
  // Loop till we find an empty entry.
  const ht_entry *ee=table->entries;
  while (ee[i].key){
    if (ee[i].key_hash==hash && !strcmp(key,ee[i].key)) return ee+i;
    // Key wasn't in this slot, move to next (linear probing).
    if (++i>=table->capacity) i=0; // At end of entries array, wrap around.
  }
  return NULL;
}
void* ht_get(ht* table, const char* key){
  const ht_entry *e=ht_get_entry(table,key);
  return e?e->value:NULL;
}

// Internal function to set an entry (without expanding table).
static ht_entry* ht_set_entry(ht_entry* ee, unsigned int capacity,const char* key, uint64_t hash, void* value, unsigned int* plength){
  unsigned int i=(unsigned int)(hash & (uint64_t)(capacity-1));
  // Loop till we find an empty entry.
  while (ee[i].key){
    if (ee[i].key_hash==hash && !strcmp(key,ee[i].key)){
      ee[i].value=value;
      if (!value) ee[i].key=NULL;
      return ee+i;
    }
    // Key wasn't in this slot, move to next (linear probing).
    if (++i>=capacity) i=0;      // At end of ee array, wrap around.
  }
  // Didn't find key, allocate+copy if needed, then insert it.
  if (plength){
    if (key && !(key=strdup(key))){
      log_error("ht.c: strdup(key)\n");
      return NULL;
    }
    (*plength)++;
  }
  ee[i].key=(char*)key;
  ee[i].key_hash=hash;
  ee[i].value=value;
  return NULL;
}
// Expand hash table to twice its current size. Return true on success,
// false if out of memory.
static bool ht_expand(ht* table){
  const unsigned int new_capacity=table->capacity<<1;
  if (new_capacity<table->capacity) return false;  // overflow (capacity would be too big)
  ht_entry* new_ee=calloc(new_capacity, sizeof(ht_entry));
  if (!new_ee){
    log_error("ht.c: calloc new_ee\n");
    return false;
  }
  for (unsigned int i=0; i<table->capacity; i++){
    const ht_entry e=table->entries[i];
    if (e.key) ht_set_entry(new_ee,new_capacity,e.key,e.key_hash,e.value,NULL);
  }
  free(table->entries);
  table->entries=new_ee;
  table->capacity=new_capacity;
  return true;
}
ht_entry* ht_set(ht* table, const char* key, void* value){
  if (!key || ((table->length>=table->capacity>>1) && !ht_expand(table))) return NULL;
  return ht_set_entry(table->entries, table->capacity,key,hash_key(key),value, &table->length);
}
//unsigned int ht_length(ht* table){  return table->length;}
/* *** Iteration over elements ***  */
ht_entry *ht_next(ht *table, int *index){
  int i=*index;
  const int c=table->capacity;
 ht_entry *ee=table->entries;
  while(i>=0 && i<c && !ee[i].key) i++;
  if (i>=c) return NULL;
  *index=i+1;
  return ee+i;
}
