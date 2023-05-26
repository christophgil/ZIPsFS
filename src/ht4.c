/*
  Copyright (C) 2023   christoph Gille
  Simple hash map
  This is library is an improved version of
     Author Ben Hoyt:  Simple hash table implemented in C.
     https://benhoyt.com/writings/hash-table-in-c/
  .
  Improvements:
   - Elememsts can be deleted.
   - Improved performance:
       + Stores the hash value of the keys
       + Keys can be stored in a pool to reduce the number of malloc invocations.
   - Can be used for int-values as keys and int values as values.
*/

#ifndef _ht_dot_c
#define _ht_dot_c
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
#define HT_INTKEY (1<<30)
/* ***************** */
/* *** keystore ***  */
/* ***************** */
struct keystore{
  char **kk;
  unsigned int l,i,b,dim;
};
#define kk ks->kk
#define i ks->i
#define b ks->b
#define l ks->l
#define dim ks->dim
// #define log_keystore(...) printf(__VA_ARGS__)
#define log_keystore(...)
/* Make sure that kk[i] is not NULL */
void keystore_inc_i(struct keystore *ks){
  if (++i>=l){
    kk=realloc(kk,(l*=2)*sizeof(void*));
    assert(kk!=NULL);
    log_keystore("Did realloc kk  l=%u\n",l);
  }
  kk[i]=malloc(dim);
  b=0;
  log_keystore("Did i++ %u\n",i);
}
static const char *keystore_push(struct keystore *ks,const char *k,const int klen){
  if (!klen || !k) return "";
  if (!kk){ /* Initialize */
    if (!dim) dim=0x1000;
    assert(8==sizeof(void*));
    (kk=malloc((l=0x100)*sizeof(void*)))[0]=malloc(dim);
    log_keystore("Did malloc kk:=%p\n",(void*)kk);
  }
  if (b+klen+1>dim){ /* Current buffer too small */
    if (klen+1>dim){ /* Key too big. Should not happen often. */
      assert(NULL!=(kk[i]=realloc(kk[i],b+klen+1)));
    }else{
      keystore_inc_i(ks);
    }
  }
  const int b0=b;
  b+=klen+1;
  log_keystore("Going to memcpy i=%u b=%u\n",i,b0);
  return memcpy(kk[i]+b0,k,klen+1);
}
static void keystore_destroy(struct keystore *ks){
  if (kk){
  for(int j=i+1;--j>=0;){
    log_keystore("Going to free kk[%d] %p\n",j,(void*)(kk+j));
    free(kk[j]);
  }
  log_keystore("Going to free kk %p\n",(void*)kk);
  free(kk);
  }
}
#undef log_keystore
#undef dim
#undef kk
#undef i
#undef b
#undef l
/* ********************************************************* */
struct ht_entry{
  uint64_t key_hash;
  const char* key;  // key is NULL if this slot is empty
  void* value;
};
#define  _HT_LDDIM 4
struct ht{
  uint32_t flags,capacity,length;
  struct ht_entry* entries;  // hash slots
  long must_be_zero;
  struct keystore keystore;
};
/* **************************************************************************
   The keystore stores the files efficiently to reduce number of mallog
   This should be used if entries in the hashtable are not going to be removed
*****************************************************************************/
bool ht_init_with_keystore(struct ht *table,uint32_t log2initalCapacity,uint32_t log2keystoreDim){
  if (!table) return false;
  if (table->must_be_zero){
    log_error("The struct ht must be initialized with zero\n");
    return false;
  }
  if (log2keystoreDim) table->keystore.dim=(1<<log2keystoreDim);
  table->flags=log2initalCapacity&0xFF000000;
  if (table->entries){
    log_debug_now("HT already initialized\n");
    return true;
  }
  //  memset(table,0,sizeof(struct ht));
  table->capacity=(log2initalCapacity&0xff)?(1<<(log2initalCapacity&0xff)):256;
  if (!(table->entries=calloc(table->capacity,sizeof(struct ht_entry)))){
    log_error("ht.c: calloc table->entries\n");
    return false;
  }
  return true;
}
bool ht_init(struct ht *table,uint32_t log2initalCapacity){
  return ht_init_with_keystore(table,log2initalCapacity,0);
}
void ht_destroy(struct ht *t){
  if (t->keystore.dim){ /* All keys are in the keystore */

    keystore_destroy(&t->keystore);
  }else if (!(t->flags&HT_INTKEY)){ /* Each key has been stored on the heap individually */

    for (int i=t->capacity;--i>=0;){
      struct ht_entry *e=t->entries+i;
      if (e) {free((void*)e->key); e->key=NULL;}
    }
  }
  free(t->entries);t->entries=NULL;
}
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL
// Return 64-bit FNV-1a hash for key (NUL-terminated). See  https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_value(const char* key){
  uint64_t hash=FNV_OFFSET;
  for (const char* p=key; *p;p++){
    hash^=(uint64_t)(unsigned char)(*p);
    hash*=FNV_PRIME;
  }
  return hash;
}
static inline bool ht_keys_equal(const struct ht *t,const char *k1,const char *k2){
  if (t->flags&HT_INTKEY){
    return k1==k2;
  }else{
    return !strcmp(k1,k2);
  }
}
const struct ht_entry* ht_get_entryh(struct ht *table, const char* key, const uint64_t hash){
  if (!table->entries) ht_init(table,8);
  // AND hash with capacity-1 to ensure it's within entries array.
  if (!table){ fprintf(stderr,"ht_get table is NULL\n");return NULL;}
  uint32_t i=(uint32_t)(hash & (uint64_t)(table->capacity - 1));
  // Loop till we find an empty entry.
  const struct ht_entry *ee=table->entries;
  while (ee[i].key){
    if (ee[i].key_hash==hash && ht_keys_equal(table,key,ee[i].key)) return ee+i;
    // Key wasn't in this slot, move to next (linear probing).
    if (++i>=table->capacity) i=0; // At end of entries array, wrap around.
  }
  return NULL;
}
//const struct ht_entry* ht_get_entry(struct ht *table, const char* key){  return key?ht_get_entryh(table,key, hash_value(key)):NULL; }
void* ht_geth(struct ht *table, const char* key,uint64_t hash){
  const struct ht_entry *e=key?ht_get_entryh(table,key,hash):NULL;
  return e?e->value:NULL;
}

void* ht_get(struct ht *table, const char* key){
  const struct ht_entry *e=key?ht_get_entryh(table,key,hash_value(key)):NULL;
  return e?e->value:NULL;
}


// Internal function to set an entry (without expanding table).
static struct ht_entry* ht_set_entry(struct ht *table,struct ht_entry* ee, uint32_t capacity,const char* key, uint64_t hash, void* value, uint32_t* plength){
  uint32_t i=(uint32_t)(hash & (uint64_t)(capacity-1));
  // Loop till we find an empty entry.
  while (ee[i].key){
    if (ee[i].key_hash==hash && ht_keys_equal(table,key,ee[i].key)){
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
      if ((table->keystore.dim)){
        key=keystore_push(&table->keystore,key,strlen(key));
      }else if (!(table->flags&HT_INTKEY) &&  !(key=strdup(key))){
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
static bool ht_expand(struct ht *table){
  const uint32_t new_capacity=table->capacity<<1;
  if (new_capacity<table->capacity) return false;  // overflow (capacity would be too big)
  struct ht_entry* new_ee=calloc(new_capacity,sizeof(struct ht_entry));
  if (!new_ee){ log_error("ht.c: calloc new_ee\n"); return false; }
  for (uint32_t i=0; i<table->capacity; i++){
    const struct ht_entry e=table->entries[i];
    if (e.key) ht_set_entry(table,new_ee,new_capacity,e.key,e.key_hash,e.value,NULL);
  }
  free(table->entries);
  table->entries=new_ee;
  table->capacity=new_capacity;
  return true;
}
struct ht_entry* ht_seth(struct ht *table, const char* key, uint64_t hash,void* value){
  if (!table->entries) ht_init(table,8);
  if (!key || ((table->length>=table->capacity>>1) && !ht_expand(table))) return NULL;
  return ht_set_entry(table,table->entries, table->capacity,key,hash,value, &table->length);
}
struct ht_entry* ht_set(struct ht *table, const char* key, void* value){
  return ht_seth(table,key,hash_value(key),value);
}
/********************************************* */
/* ***  Int key int value                  *** */
/* ***  Limitation: key2 must not be zero  *** */
/********************************************* */
void ht_set_int(struct ht *table, const uint64_t key1, const uint64_t key2, const uint64_t value){
  table->flags|=HT_INTKEY;
  assert(key2!=0);
  ht_seth(table,(char*)key2,key1,(void*)value);
}
uint64_t ht_get_int(struct ht *table, uint64_t const key1, uint64_t const key2){
    table->flags|=HT_INTKEY;
  return (uint64_t) ht_geth(table,(char*)key2, key1);
}
//uint32_t ht_length(struct ht *table){  return table->length;}
/* *** Iteration over elements ***  */
struct ht_entry *ht_next(struct ht *table, int *index){
  int i=*index;
  const int c=table->capacity;
  struct ht_entry *ee=table->entries;
  if (!ee) return NULL;
  while(i>=0 && i<c && !ee[i].key) i++;
  if (i>=c) return NULL;
  *index=i+1;
  return ee+i;
}
/* identical Strings will have same address */
const char* ht_internalize_strg(struct ht *table,const char *s){
  if (!s) return NULL;
  const char *s2=ht_get(table,s);
  if (s2) return s2;
  ht_set(table,s,(void*)s);
  return s;
}
#endif
#define _ht_dot_c_end
// 1111111111111111111111111111111111111111111111111111111111111
#if __INCLUDE_LEVEL__ == 0
void ht_test_keystore(int argc, char *argv[]){
  struct keystore ks={0};
  ks.dim=10;
  char **copy=malloc(argc*8);
  for(int i=1;i<argc;i++){
    printf("i=%d argv=%s\n",i,argv[i]);
    char *a=argv[i];
    assert(ks.dim>0);
    copy[i]=(char*)keystore_push(&ks,a,strlen(a));
    assert(ks.dim>0);
    if (strcmp(a,copy[i])){ printf("Error\n");return;}
  }
  if (1){
    for(int i=1;i<argc;i++){
      printf("%d\t%s\t%s\n",i,argv[i],copy[i]);
    }
    log_debug_now("GOING TO keystore_destroy\n");
    keystore_destroy(&ks);
  }
  free(copy);
}

void ht_test1(int argc, char *argv[]){
  struct ht no_dups={0};
  ht_init_with_keystore(&no_dups,7,4);
  char *VV[]={"A","B","C","D","E","F","G","H","I"};
  const int L=9;
  for(int i=1;i<argc;i++){
    char *a=argv[i];
    ht_set(&no_dups,a,VV[i%L]);
  }
  for(int i=1;i<argc;i++){
    char *fetched=(char*)ht_get(&no_dups,argv[i]);
    printf("argv[i]=%s  s=%s\n",argv[i],fetched);
    assert(!strcmp(fetched,VV[i%L]));
  }
  if (0){
    int index=0;
    struct ht_entry *e;
    while((e=ht_next(&no_dups,&index))){
      printf("iter %s=%s  ",e->key,(char*)e->value);
    }
  }
  ht_destroy(&no_dups);
}
void test_int(int argc, char *argv[]){
  const int n=atoi(argv[1]);
  struct ht table={0};
  ht_init(&table,7);
  for(int set=2;--set>=0;){
    for(int i=-n;i<n;i++){
      for(int j=-n;j<n;j++){
        if (j==0) continue;
        const int u=i*i+j*j;
        if (set){
          ht_set_int(&table,i,j,i*i+j*j);
        }else{
          const uint64_t v=ht_get_int(&table,i,j);
          printf("%s%d\t%d\t%d\t%lu\n"ANSI_RESET,u!=v?ANSI_FG_RED:"", i,j,u,v);
        }
      }
    }
  }
  ht_destroy(&table);
}
int main(int argc, char *argv[]){
  //  ht_test1(argc,argv);
  //printf(" sizeof(void*)=%zu\n",sizeof(void*));return 0;
  //ht_test_keystore(argc,argv);
    //test_int(argc,argv);
  {
      struct ht no_dups={0};
  ht_init_with_keystore(&no_dups,8,12);
  ht_destroy(&no_dups);
  }
}
#endif
