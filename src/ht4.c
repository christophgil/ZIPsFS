// Simple hash table implemented in C.
// https://benhoyt.com/writings/hash-table-in-c/
// Author Ben Hoyt
// Modified by Christoph Gille
//   (global-set-key (kbd "<f4>") '(lambda() (interactive) (switch-to-buffer "ht4.c")))
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
#include "cg_log.c"
       #include <sys/types.h>
       #include <unistd.h>

#ifndef log_error
#define log_error(...)  printf("\x1B[31m Error \x1B[0m"),fprintf(stderr,__VA_ARGS__)
#endif


#define HT_STORE_KEYS_IN_POOL (1<<30)
//#define HT_INIT_IF_NOT_ALREADY (1<<30)
#define _HT_STORE_KEYS_MAX 16
#define MY_UINT unsigned int


const char *HT_FAKE_KEY="HT_FAKE_KEY";

typedef struct ht ht;
typedef struct {
  uint64_t key_hash;
  const char* key;  // key is NULL if this slot is empty
  void* value;
} ht_entry;
#define  _HT_LDDIM 4
struct ht {
  MY_UINT flags,capacity,length;
  ht_entry* entries;  // hash slots
  char ***keys;
  int keys_l,keys_i;
  long must_be_zero;
};


static bool _ht_pushKey_p(const char *k,int klen){ return klen<(1<<(_HT_LDDIM-1)); }

static char *_ht_pushKey(ht *t,const char *k,int klen){
  assert(0);
  const char debug=false;
  const MY_UINT DIM=1<<_HT_LDDIM,AND=(DIM-1);
  if(debug)log_debug_now("DIM=%u  AND=%x \n",DIM,AND);
  MY_UINT b=t->keys_i,e=b+klen+1;
  if ((b&~AND)!=(e&~AND)){ b=(e&~AND)+DIM; e=b+klen+1; if(debug)log_debug_now("next dimenstion \n"); } /* Does not fit any more. Reset d0, Increment d1. */
  t->keys_i=e;
  const int
    d2=AND&(b>>(2*_HT_LDDIM)),
    d1=AND&(b>>(1*_HT_LDDIM)),
    d0=AND&b;
  char ***kkk=t->keys;
  if (!kkk) { kkk=t->keys=calloc(t->keys_l=16,sizeof(void*)); if(debug)log_debug_now("kkk=calloc \n");}
  if (d2>=t->keys_l) { kkk=t->keys=realloc(kkk,(t->keys_l*=2)*sizeof(void*)); if(debug)log_debug_now("kkk=t->keys=realloc\n");}
  //    log_debug_now("d012=%d,%d,%d ?????????????\n",d0,d1,d2);
  if (!kkk[d2]) { kkk[d2]=calloc(DIM,sizeof(void*)); if(debug)log_debug_now("kkk[d2]=calloc\n");}
  if (!kkk[d2][d1]) { kkk[d2][d1]=calloc(DIM,1); if(debug)log_debug_now("kkk[d2][d1]=calloc\n");}
  char *s=kkk[d2][d1]+(b&AND);
  memcpy(s,k,klen+1);
  if(debug)log_debug_now("k=%s be=%u-%u d012=%d,%d,%d s=%s k=%s\n",k,b,e,d0,d1,d2,s,k);
  return s;
}

void _ht_destroy_keys(ht *t){
  assert(false);
  const MY_UINT DIM=1<<_HT_LDDIM;
  const char debug=false;
  if(debug)log_entered_function("ht_destroy_keys\n");
  char ***kkk=t->keys;
  if (kkk){
    for(int i2=t->keys_l;--i2>=0;){
      char **kk=kkk[i2];
      if (kk){
        for(int i1=DIM;--i1>=0;){
          char *k=kk[i1];
          if (k) {free(k); if(debug)log_debug_now("free k ");}
        }
        free(kk);
        if(debug)log_debug_now("\nfree kk \n");
      }
    }
    free(kkk);
    if(debug)log_debug_now("\nfree kkk \n");
  }
  if(debug)log_exited_function("ht_destroy_keys\n");
}

bool ht_init(ht *table,MY_UINT log2initalCapacity){
  if (!table) return false;
  if (table->must_be_zero){
    log_error("The struct ht must be initialized with zero\n");
    exit(1);
  }
  table->flags=log2initalCapacity&0xFF000000;
  if (table->entries){
    log_debug_now("HT already initialized\n");
    return true;
  }
  //  memset(table,0,sizeof(struct ht));
  table->capacity=(log2initalCapacity&0xff)?(1<<(log2initalCapacity&0xff)):256;
  if (!(table->entries=calloc(table->capacity,sizeof(ht_entry)))){
    log_error("ht.c: calloc table->entries\n");
    return false;
  }
  return true;
}
void ht_destroy(ht* t){
  const bool pool=(t->flags&HT_STORE_KEYS_IN_POOL);
  assert(!pool);
  if (pool) _ht_destroy_keys(t);
  else{
    for (int i=t->capacity;--i>=0;){
      ht_entry *e=t->entries+i;
      if (e) {free((void*)e->key); e->key=NULL;}
    }
  }
  free(t->entries);t->entries=NULL;
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
const ht_entry* ht_get_entryh(ht* table, const char* key, uint64_t hash){
  if (!table->entries) ht_init(table,8);
  // AND hash with capacity-1 to ensure it's within entries array.
  if (!table){ fprintf(stderr,"ht_get table is NULL\n");return NULL;}
  MY_UINT i=(MY_UINT)(hash & (uint64_t)(table->capacity - 1));
  // Loop till we find an empty entry.
  const ht_entry *ee=table->entries;
  const bool hash_only=(key==HT_FAKE_KEY);
  while (ee[i].key){
    if (ee[i].key_hash==hash && (hash_only||!strcmp(key,ee[i].key))) return ee+i;
    // Key wasn't in this slot, move to next (linear probing).
    if (++i>=table->capacity) i=0; // At end of entries array, wrap around.
  }
  return NULL;
}
//const ht_entry* ht_get_entry(ht* table, const char* key){  return key?ht_get_entryh(table,key, hash_key(key)):NULL; }

void* ht_geth(ht* table, const char* key,uint64_t hash){
  const ht_entry *e=key?ht_get_entryh(table,key,hash):NULL;
  return e?e->value:NULL;
}


void* ht_get(ht* table, const char* key){
    const ht_entry *e=key?ht_get_entryh(table,key,hash_key(key)):NULL;
  return e?e->value:NULL;
}
void* ht_getn(ht* table, uint64_t key){
  const ht_entry *e=ht_get_entryh(table,HT_FAKE_KEY,key);
  return e?e->value:NULL;
}



// Internal function to set an entry (without expanding table).
static ht_entry* ht_set_entry(ht *table,ht_entry* ee, MY_UINT capacity,const char* key, uint64_t hash, void* value, MY_UINT* plength){

  MY_UINT i=(MY_UINT)(hash & (uint64_t)(capacity-1));
  // Loop till we find an empty entry.
  while (ee[i].key){
    if (ee[i].key_hash==hash && !strcmp(key,ee[i].key)){
      ee[i].value=value;
      if (!value) { free((char*)ee[i].key);ee[i].key=NULL;}
      return ee+i;
    }
    // Key wasn't in this slot, move to next (linear probing).
    if (++i>=capacity) i=0;      // At end of ee array, wrap around.
  }
  // Didn't find key, allocate+copy if needed, then insert it.
  if (plength){
    if (key){
      int len;
      if ((table->flags&HT_STORE_KEYS_IN_POOL) && _ht_pushKey_p(key,len=strlen(key))){
        key=_ht_pushKey(table,key,len);
      }else if (!(key=strdup(key))){
        log_error("ht.c: strdup(key)\n");
        return NULL;
      }
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
  const MY_UINT new_capacity=table->capacity<<1;
  if (new_capacity<table->capacity) return false;  // overflow (capacity would be too big)
  ht_entry* new_ee=calloc(new_capacity,sizeof(ht_entry));
  if (!new_ee){ log_error("ht.c: calloc new_ee\n"); return false; }
  for (MY_UINT i=0; i<table->capacity; i++){
    const ht_entry e=table->entries[i];
    if (e.key) ht_set_entry(table,new_ee,new_capacity,e.key,e.key_hash,e.value,NULL);
  }
  free(table->entries);
  table->entries=new_ee;
  table->capacity=new_capacity;
  return true;
}

ht_entry* ht_seth(ht* table, const char* key, uint64_t hash,void* value){
  if (!hash && key!=HT_FAKE_KEY) hash=hash_key(key);
  if (!table->entries) ht_init(table,8);
  if (!key || ((table->length>=table->capacity>>1) && !ht_expand(table))) return NULL;
  return ht_set_entry(table,table->entries, table->capacity,key,hash,value, &table->length);
}
ht_entry* ht_set(ht* table, const char* key, void* value){ return ht_seth(table,key,0,value);}
ht_entry* ht_setn(ht* table, uint64_t key,void* value){ return ht_seth(table,HT_FAKE_KEY,key,value);}


//MY_UINT ht_length(ht* table){  return table->length;}
/* *** Iteration over elements ***  */
ht_entry *ht_next(ht *table, int *index){
  int i=*index;
  const int c=table->capacity;
  ht_entry *ee=table->entries;
  if (!ee) return NULL;
  while(i>=0 && i<c && !ee[i].key) i++;
  if (i>=c) return NULL;
  *index=i+1;
  return ee+i;
}



/* identical Strings will have same address */
const char* ht_internalize_strg(ht *table,const char *s){
  if (!s) return NULL;
  const char *s2=ht_get(table,s);
  if (s2) return s2;
  ht_set(table,s,(void*)s);
  return s;
}
