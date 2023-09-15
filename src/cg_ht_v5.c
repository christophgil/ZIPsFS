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
#define ANSI_RESET "\x1B[0m"
#define ANSI_FG_RED "\x1B[31m"
#define ANSI_FG_GREEN "\x1B[32m"

#ifndef _ht_dot_c
#define _ht_dot_c
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <malloc.h>
#include <sys/types.h>
#include <unistd.h>
#include "cg_mstore_v2.c"
#define log_ht_error(...)  fprintf(stderr,"\x1B[31m Error \x1B[0m"),fprintf(stderr,__VA_ARGS__)

#define HT_FLAG_INTKEY (1U<<30)
#define HT_FLAG_KEYSTORE (1U<<29)
/* ********************************************************* */
struct ht_entry{
  uint64_t key_hash;
  size_t key_len;
  const char* key;  // key is NULL if this slot is empty
  void* value;
};
#define S_HT_ENTRY sizeof(struct ht_entry)
#define  _HT_LDDIM 4
#define _STACK_HT_ENTRY ULIMIT_S
struct ht{
  uint32_t flags,capacity,length;

  struct ht_entry* entries,_stack_ht_entry[_STACK_HT_ENTRY];  // hash slots
  struct mstore keystore;
};
/* **************************************************************************
   The keystore stores the files efficiently to reduce number of mallog
   This should be used if entries in the hashtable are not going to be removed
*****************************************************************************/

static bool ht_init_with_keystore(struct ht *ht,uint32_t flags,uint32_t log2initalCapacity,uint32_t log2keystoreDim){
  memset(ht,0,sizeof(struct ht));
  if (log2keystoreDim)  mstore_init(&ht->keystore,NULL,1<<log2keystoreDim);
  ht->flags=flags|(log2initalCapacity&0xFF000000)|(log2keystoreDim?HT_FLAG_KEYSTORE:0);
#define C ht->capacity
  if (!C){
    if ((C=(log2initalCapacity&0xff)?(1<<(log2initalCapacity&0xff)):0)>_STACK_HT_ENTRY/2){
      if (!(ht->entries=calloc(C,S_HT_ENTRY))){log_ht_error("ht.c: calloc ht->entries\n");return false;}
    }else{
      ht->entries=ht->_stack_ht_entry;
      C=_STACK_HT_ENTRY;
    }
  }
#undef C
  return true;
}
static bool ht_init(struct ht *ht,uint32_t flags,uint32_t log2initalCapacity){
  return ht_init_with_keystore(ht,flags,log2initalCapacity,0);
}
static void _ht_free_entries(struct ht *ht){
  if (ht && ht->entries!=ht->_stack_ht_entry){
    free(ht->entries);
    ht->entries=NULL;
  }
}
static void ht_destroy(struct ht *ht){
  if (ht->keystore.dim){ /* All keys are in the keystore */
    mstore_destroy(&ht->keystore);
  }else if (!(ht->flags&HT_FLAG_INTKEY)){ /* Each key has been stored on the heap individually */
    for (int i=ht->capacity;--i>=0;){
      struct ht_entry *e=ht->entries+i;
      if (e) {free((void*)e->key); e->key=NULL;}
    }
  }
  _ht_free_entries(ht);
}
static inline bool ht_keys_equal(const struct ht *t,const char *k1, int len1,const char *k2,int len2){
  if (t->flags&HT_FLAG_INTKEY) return k1==k2;
  return len1==len2 && k1 && k2 && !memcmp(k1,k2,len1);
}
static const struct ht_entry* ht_get_entry(struct ht *ht, const char* key,int len, uint64_t hash){
  assert(ht!=NULL && ht->entries!=NULL);
  if (!len &&  !(ht->flags&HT_FLAG_INTKEY)) len=strlen(key);
  if (!hash && !(ht->flags&HT_FLAG_INTKEY)) hash=hash_value(key,len);
  uint32_t i=(uint32_t)(hash&(uint64_t)(ht->capacity-1));
  // Loop till we find an empty entry.
  const struct ht_entry *ee=ht->entries;
  while (ee[i].key){
    if (ee[i].key_hash==hash && ht_keys_equal(ht,key,len,ee[i].key,ee[i].key_len)) return ee+i;
    // Key wasn't in this slot, move to next (linear probing).
    if (++i>=ht->capacity) i=0; // At end of entries array, wrap around.
  }
  return NULL;
}

/*
  Both, len and hash can be  zero. In this case  they are determined.
 */
static void* ht_get(struct ht *ht, const char* key,int len,uint64_t hash){
  if (!key || !ht)return NULL;
  const struct ht_entry *e=ht_get_entry(ht,key,len,hash);
  return e?e->value:NULL;
}
// Internal function to set an entry (without expanding ht).
static struct ht_entry* _ht_set_entry(struct ht *ht,struct ht_entry* ee, size_t capacity,const char* key,int len, uint64_t hash, void* value, uint32_t* plength){
  uint32_t i=(uint32_t)(hash&(capacity-1));
  // Loop till we find an empty entry.
  while (ee[i].key){
    if (ee[i].key_hash==hash && ht_keys_equal(ht,key,len,ee[i].key,ee[i].key_len)){
      ee[i].value=value;
      if (!value){
        if (!(ht->flags&HT_FLAG_KEYSTORE)) free((char*)ee[i].key);
        ee[i].key=NULL;
      }
      return ee+i;
    }
    // Key wasn't in this slot, move to next (linear probing).
    if (++i>=capacity) i=0;      // At end of ee array, wrap around.
  }
  // Didn't find key, allocate+copy if needed, then insert it.
  if (plength){
    if (key){
      if ((ht->flags&HT_FLAG_KEYSTORE)){
        //assert(hash_value(key)==hash);
        key=mstore_addstr(&ht->keystore,key,len);
        //assert(hash_value(key)==hash);
      }else if (!(ht->flags&HT_FLAG_INTKEY) &&  !(key=strdup(key))){
        log_ht_error("ht.c: strdup(key)\n");
        return NULL;
      }
    }
    (*plength)++;
  }
  ee[i].key=(char*)key;
  ee[i].key_len=len;
  ee[i].key_hash=hash;
  ee[i].value=value;
  return NULL;
}
// Expand hash ht to twice its current size. Return true on success,
// false if out of memory.
static bool ht_expand(struct ht *ht){
  const uint32_t new_capacity=ht->capacity<<1;
  if (new_capacity<ht->capacity) return false;  // overflow (capacity would be too big)
  struct ht_entry* new_ee=calloc(new_capacity,S_HT_ENTRY);
  if (!new_ee){ log_ht_error("ht.c: calloc new_ee\n"); return false; }
  for (uint32_t i=0; i<ht->capacity; i++){
    const struct ht_entry e=ht->entries[i];
    if (e.key) _ht_set_entry(ht,new_ee,new_capacity,e.key,e.key_len,e.key_hash,e.value,NULL);
  }
  _ht_free_entries(ht);
  ht->entries=new_ee;
  ht->capacity=new_capacity;
  return true;
}
static struct ht_entry* ht_set(struct ht *ht, const char* key,int len, uint64_t hash,void* value){
  assert (ht!=NULL && ht->entries!=NULL);
    if (!key)return NULL;
    if (!len && !(ht->flags&HT_FLAG_INTKEY)) len=strlen(key);
  if (((ht->length>=ht->capacity>>1) && !ht_expand(ht))) return NULL;
  if (!hash  && !(ht->flags&HT_FLAG_INTKEY)) hash=hash_value(key,len);
  return _ht_set_entry(ht,ht->entries, ht->capacity,key,len,hash,value, &ht->length);
}
/********************************************* */
/* ***  Int key int value                  *** */
/* ***  Limitation: key2 must not be zero  *** */
/********************************************* */
static void ht_set_int(struct ht *ht, const uint64_t key1, const uint64_t key2, const uint64_t value){
  ht->flags|=HT_FLAG_INTKEY;
  assert(key2!=0);
  ht_set(ht,(char*)key2,0,key1,(void*)value);
}
static uint64_t ht_get_int(struct ht *ht, uint64_t const key1, uint64_t const key2){
  ht->flags|=HT_FLAG_INTKEY;
  return (uint64_t) ht_get(ht,(char*)key2,0,key1);
}
//uint32_t ht_length(struct ht *ht){  return ht->length;}
/* *** Iteration over elements ***  */
static struct ht_entry *ht_next(struct ht *ht, int *index){
  int i=*index;
  const int c=ht->capacity;
  struct ht_entry *ee=ht->entries;
  if (!ee) return NULL;
  while(i>=0 && i<c && !ee[i].key) i++;
  if (i>=c) return NULL;
  *index=i+1;
  return ee+i;
}
static void ht_clear(struct ht *ht){
  struct ht_entry *e=ht->entries;
  if (e){
    memset(e,0,ht->capacity*S_HT_ENTRY);
    mstore_clear(&ht->keystore);
  }
}

#if 0
/* identical Strings will have same address */
static const char* ht_internalize_strg(struct ht *ht,const char *s){
  if (!s) return NULL;
  const char *s2=ht_get(ht,s);
  if (s2) return s2;
  ht_set(ht,s,(void*)s);
  return s;
}
#endif
#endif
#define _ht_dot_c_end
// 1111111111111111111111111111111111111111111111111111111111111
#if __INCLUDE_LEVEL__ == 0
static void debug_print_keys(struct ht *ht){
  const struct ht_entry *ee=ht->entries;
  for(uint32_t i=0;i<ht->capacity;i++){
    const char *k=ee[i].key;
    if (k){
      printf("debug_print_keys %u\t%s\n",i,k);
      if (hash_value(k,strlen(k))!=ee[i].key_hash){ printf("HHHHHHHHHHHHHHHHHHHHHHHH!\n");exit(1);}
    }
  }
}
static void test_ht_1(int argc, const char *argv[]){
  struct ht ht={0};
  ht_init_with_keystore(&ht,0,7,4);
  //ht_init(&ht,7);
  char *VV[]={"A","B","C","D","E","F","G","H","I"};
  const int L=9;
  for(int i=1;i<argc;i++){
    ht_set(&ht,argv[i],0,0,VV[i%L]);
  }
  //  debug_print_keys(&ht);
  for(int i=1;i<argc;i++){
    char *fetched=(char*)ht_get(&ht,argv[i],0,0);
    printf("argv[i]=%s  s=%s\n",argv[i],fetched);
    if (!fetched){printf(" !!!! fetched is NULL\n"); exit(9);}
    if (fetched && strcmp(fetched,VV[i%L])){printf(" !!!! fetched strcmp NULL\n"); exit(9);}
  }
  ht_destroy(&ht);
}
static void test_int(int argc, const char *argv[]){
  const int n=atoi(argv[1]);
  struct ht ht={0};
  ht_init(&ht,HT_FLAG_INTKEY,7);
  for(int set=2;--set>=0;){
    for(int i=-n;i<n;i++){
      for(int j=-n;j<n;j++){
        if (j==0) continue;
        const int u=i*i+j*j;
        if (set){
          ht_set_int(&ht,i,j,u);
        }else{
          const uint64_t v=ht_get_int(&ht,i,j);
          printf("%s%d\t%d\t%d\t%"PRIu64"\n"ANSI_RESET,   u!=v?ANSI_FG_RED:ANSI_FG_GREEN, i,j,u,v);
          assert(u==v);
        }
      }
    }
  }
  ht_destroy(&ht);
}
int main(int argc, const char *argv[]){
  test_int(argc,argv);
  //test_ht_1(argc,argv);

  if (0){
    struct ht ht={0};
    ht_init_with_keystore(&ht,0,8,12);
    ht_destroy(&ht);
  }
}
#endif
