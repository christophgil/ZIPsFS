/*
  Copyright (C) 2023   christoph Gille
  Simple hash map
  The algorithm of the hash table was inspired by the Racko Game and by  Ben Hoyt:
  See https://benhoyt.com/writings/hash-table-in-c/
  .
  Usage:
  - Include this file
  .
  Features:
  - Elememsts can be deleted.
  - High performance:
  - Keys can be stored in a pool to reduce the number of malloc calls.
  - Can be used as a hashmap char* -> void*  or     {int64,int64} -> int64
  - Iterator
  .
  Dependencies:
  - cg_mstore_v?.c
*/
#ifndef _ht_dot_c
#define _ht_dot_c
#define IF_HT_DEBUG(a) if (ht->flags&HT_FLAG_DEBUG) {a}
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stddef.h>
//#include <malloc.h>
#include <unistd.h>
#include "cg_mstore_v2.c"
#define HT_FLAG_NUMKEY (1U<<30)
#define HT_FLAG_KEYS_ARE_STORED_EXTERN (1U<<29)
#define HT_FLAG_DEBUG (1U<<28)
#define HT_FLAG_BINARY_KEY (1U<<27)
typedef uint32_t ht_keylen_t;
#define HT_KEYLEN_MAX UINT32_MAX
#define HT_KEYLEN_HASH(len,hash) ((((uint64_t)len)<<32)|hash)
#define HT_KEYLEN_SHIFT 32
#define HT_ENTRY_IS_EMPTY(e) (!(e)->key && !(e)->keylen_hash && !(e)->value)
#define _HT_HASH() if (!hash) hash=hash32(key,key_l);
#define HT_MEMALIGN_FOR_STRG 0
/* ********************************************************* */
struct ht_entry{
  uint64_t keylen_hash; /* upper 32 bits keylen  lower 32 bits hash */
  const char* key;  /* key  and keylen_hash are NULL if this slot is empty */
  void* value;
};
#define _HT_LDDIM 4
#define _STACK_HT_ENTRY ULIMIT_S
struct ht{
  const char *name;
  int id;
  uint32_t flags,capacity,length;
  struct ht_entry entry_zero,*entries,_stack_ht_entry[_STACK_HT_ENTRY];

  struct mstore keystore_buf, *keystore, *value_store;
#if defined CG_THREAD_FIELDS
  CG_THREAD_FIELDS;
#endif

  int client_value_int[3];
};
#ifdef CG_THREAD_METHODS_HT
CG_THREAD_METHODS_HT(ht);
#endif

/* **************************************************************************
   The keystore stores the files efficiently to reduce number of mallog
   This should be used if entries in the hashtable are not going to be removed
*****************************************************************************/
#define ht_init_interner(ht,flags_log2initalCapacity,mstore_dim)   _ht_init_with_keystore(ht,NULL,0,flags_log2initalCapacity|HT_FLAG_KEYS_ARE_STORED_EXTERN,NULL,mstore_dim)
#define ht_init_interner_file(ht,name,instance,flags_log2initalCapacity,mstore_dim)   _ht_init_with_keystore(ht,name,instance,flags_log2initalCapacity|HT_FLAG_KEYS_ARE_STORED_EXTERN,NULL,mstore_dim|MSTORE_OPT_MMAP_WITH_FILE)
#define ht_init_with_keystore_dim(ht,flags_log2initalCapacity,mstore_dim) _ht_init_with_keystore(ht,NULL,0,flags_log2initalCapacity,NULL,mstore_dim)
#define ht_init_with_keystore_file(ht,name,instance,flags_log2initalCapacity,mstore_dim) _ht_init_with_keystore(ht,name,instance,flags_log2initalCapacity,NULL,mstore_dim|MSTORE_OPT_MMAP_WITH_FILE)
#define ht_init_with_keystore(ht,flags_log2initalCapacity,m)              _ht_init_with_keystore(ht,NULL,0,flags_log2initalCapacity,m,0)
#define ht_init(ht,flags_log2initalCapacity)                              _ht_init_with_keystore(ht,NULL,0,flags_log2initalCapacity,NULL,0)
static struct ht *_ht_init_with_keystore(struct ht *ht,const char *name,  int id,uint32_t flags_log2initalCapacity, struct mstore *m, uint32_t mstore_dim){
  memset(ht,0,sizeof(struct ht));
  if (m){
    ht->keystore=m;
    if (!name) name=m->name;
    if (!id) id=m->id;
  }else if(mstore_dim){
    if (mstore_dim&MSTORE_OPT_MMAP_WITH_FILE) ASSERT(name!=NULL); /* Needed for file */
    _mstore_init(ht->keystore=&ht->keystore_buf,name,id,mstore_dim);
  }
  ht->name=name;
  ht->id=id;
  ht->flags=(flags_log2initalCapacity&0xFF000000);
#define C ht->capacity
  if ((C=(flags_log2initalCapacity&0xff)?(1<<(flags_log2initalCapacity&0xff)):0)>_STACK_HT_ENTRY/2){
    if (!(ht->entries=calloc(C,sizeof(struct ht_entry)))){log_error0("ht.c: calloc ht->entries\n");return NULL;}
  }else{
    memset(ht->entries=ht->_stack_ht_entry,0,sizeof(struct ht_entry)*_STACK_HT_ENTRY);
    C=_STACK_HT_ENTRY;
  }
#undef C
  return ht;
}

static void _ht_free_entries(struct ht *ht){
  if (ht && ht->entries!=ht->_stack_ht_entry){
    free(ht->entries);
    ht->entries=NULL;
  }
}
static void ht_destroy(struct ht *ht){
  CG_THREAD_OBJECT_ASSERT_LOCK(ht);
  if (ht->keystore && ht->keystore->capacity){ /* All keys are in the keystore */
    mstore_destroy(ht->keystore);
  }else if (!(ht->flags&(HT_FLAG_NUMKEY|HT_FLAG_KEYS_ARE_STORED_EXTERN))){ /* Each key has been stored on the heap individually */
    RLOOP(i,ht->capacity){
      struct ht_entry *e=ht->entries+i;
      if (e){ free((void*)e->key); e->key=NULL;}
    }
  }
  _ht_free_entries(ht);
  struct mstore *m=ht->value_store;
  if (m){
    ht->value_store=NULL;
    mstore_destroy(m);
  }
}
static void ht_clear(struct ht *ht){
  CG_THREAD_OBJECT_ASSERT_LOCK(ht);
  struct ht_entry *e=ht->entries;
  if (e){
    memset(e,0,ht->capacity*sizeof(struct ht_entry));
    mstore_clear(ht->keystore);
  }
}


static MAYBE_INLINE int debug_count_empty(struct ht_entry *ee, const uint32_t capacity){
  int c=0;
  RLOOP(i,capacity) if (HT_ENTRY_IS_EMPTY(ee+i)) c++;
  return c;
}
static struct ht_entry* _ht_get_entry_ee(struct ht_entry *ee, const uint32_t capacity, const bool intkey,const char* key, const uint64_t keylen_hash){
  struct ht_entry *e=ee+(keylen_hash&(capacity-1));
#define _HT_AT_END_OF_ENTRIES_WRAP_AROUND() if (++e>=ee+capacity){e=ee;ASSERT(!count_wrap_around++);}
#define _HT_LOOP_TILL_EMPTY_ENTRY(cond)  int count_wrap_around=0;while (e->key cond)
  if (intkey){
    _HT_LOOP_TILL_EMPTY_ENTRY(|| e->keylen_hash || e->value){
      if (e->keylen_hash==keylen_hash && key==e->key) break;
      _HT_AT_END_OF_ENTRIES_WRAP_AROUND();
    }
  }else{
    ASSERT(key!=NULL);
    _HT_LOOP_TILL_EMPTY_ENTRY(){
      if (e->key && e->keylen_hash==keylen_hash && (key==e->key || !memcmp(e->key,key,keylen_hash>>HT_KEYLEN_SHIFT))) break;
      _HT_AT_END_OF_ENTRIES_WRAP_AROUND();
    }
  }
#undef _HT_LOOP_TILL_EMPTY_ENTRY
#undef _HT_AT_END_OF_ENTRIES_WRAP_AROUND
  return e;
}

/*********************************************************************************** */
/* *** Expand capacity  to twice its current size. Return true on success        *** */
/* *** Return -1: out of memory.  0: No change  1: Changed                       *** */
/*********************************************************************************** */
static int _ht_expand(struct ht *ht){
  if (ht->length<(ht->capacity>>1)) return 0;
  const uint32_t new_capacity=ht->capacity<<1;
  if (new_capacity<ht->capacity) return -1;  /* overflow (capacity would be too big) */
  struct ht_entry* new_ee=calloc(new_capacity,sizeof(struct ht_entry));
  if (!new_ee){ log_error0("ht.c: calloc new_ee\n"); return -1; }
  for(uint32_t i=0; i<ht->capacity; i++){
    const struct ht_entry *e=ht->entries+i;
    if (e->key || e->keylen_hash){
      *_ht_get_entry_ee(new_ee,new_capacity,0!=(ht->flags&HT_FLAG_NUMKEY),e->key,e->keylen_hash)=*e;
    }
  }
  _ht_free_entries(ht);
  ht->entries=new_ee;
  ht->capacity=new_capacity;
  return 1;
}

/**************************************************************************************** */
/* ***                                                                                *** */
/* ***  Functions where Strings as keys. This is the most comon usage.                *** */
/* ***  Strings do not need to be zero terminated because the length is provided.     *** */
/* ***  If hash is zero, it will be computed by the functions.                        *** */
/**************************************************************************************** */
#define _ht_get_entry_maybe_empty(ht,key,keylen_hash) _ht_get_entry_ee((ht)->entries,(ht)->capacity,0!=((ht)->flags&HT_FLAG_NUMKEY),(key),(keylen_hash))
#define E() _ht_get_entry_maybe_empty(ht,key,keylen_hash)
#define let_e_get_entry(ht,key,keylen_hash) CG_THREAD_OBJECT_ASSERT_LOCK(ht);ASSERT(key!=NULL); ASSERT(ht!=NULL); ASSERT(ht->entries!=NULL);_HT_HASH();  const uint64_t keylen_hash=HT_KEYLEN_HASH(key_l,hash);struct ht_entry *e=E()
static const char* _newKey(struct ht *ht,const char *key,uint64_t keylen_hash){
  if (0!=(ht->flags&HT_FLAG_KEYS_ARE_STORED_EXTERN)) return key;
  return ht->keystore?mstore_addstr(ht->keystore,key,keylen_hash>>HT_KEYLEN_SHIFT):strdup(key);
}
static struct ht_entry* ht_get_entry(struct ht *ht, const char* key,const ht_keylen_t key_l,ht_hash_t hash,const bool create){
  let_e_get_entry(ht,key,keylen_hash);
  if (create && HT_ENTRY_IS_EMPTY(e)){
    if (_ht_expand(ht)==1) e=E();
    e->key=_newKey(ht,key,e->keylen_hash=keylen_hash);
    ht->length++;
  }
  return e;
}
static void ht_clear_entry(struct ht_entry *e){
  if (e){
    e->key=e->value=NULL;
    e->keylen_hash=0;
  }
}
static struct ht_entry* ht_remove(struct ht *ht,const char* key,const ht_keylen_t key_l, ht_hash_t hash ){
  let_e_get_entry(ht,key,keylen_hash);
  if (e->key || e->keylen_hash==keylen_hash){
    if (!ht->keystore) free((char*)e->key);
    ht_clear_entry(e);
    ht->length--;
  }
  return e;
}
static void *ht_set(struct ht *ht,const char* key,const ht_keylen_t key_l,ht_hash_t hash, const void* value){
  let_e_get_entry(ht,key,keylen_hash);
  if (!e->key && !e->keylen_hash){ /* Didn't find key, allocate+copy if needed, then insert it. */
    if (_ht_expand(ht)==1) e=E();
    if (key && !(ht->flags&(HT_FLAG_KEYS_ARE_STORED_EXTERN|HT_FLAG_NUMKEY))){
      if (!(key=_newKey(ht,key,keylen_hash))){
        log_error("ht.c: Store key with %s\n",ht->keystore?"keystore":"strdup");
        return NULL;
      }
    }
    ht->length++;
    e->key=key;
    e->keylen_hash=keylen_hash;
  }
  void *old=e->value;
  e->value=(void*)value;
  return old;
}
static void* ht_get(struct ht *ht, const char* key,const ht_keylen_t key_l,ht_hash_t hash){
  if (!key) return NULL;
  const struct ht_entry *e=ht_get_entry(ht,key,key_l,hash,false);
  return e->key?e->value:NULL;
}
#undef let_e_get_entry
#undef E

/******************************** */
/* ***  Internalize Strings   *** */
/******************************** */
/* memoryalign is  [ 1,2,4,8] or 0 forString
   See https://docs.rs/string-interner/latest/string_interner/
   https://en.wikipedia.org/wiki/String_interning
*/
const void *ht_intern(struct ht *ht,const void *bytes,const off_t bytes_l,ht_hash_t hash,const int memoryalign){
  ASSERT(ht->keystore!=NULL);
  if (!bytes || mstore_contains(ht->keystore,bytes)) return bytes;
  if (memoryalign==HT_MEMALIGN_FOR_STRG && !*(char*)bytes) return "";
  if (!hash) hash=hash32(bytes,bytes_l);
  struct ht_entry *e=ht_get_entry(ht,bytes,bytes_l,hash,true);
  if (!e->value){
    e->key=e->value=(void*)(memoryalign==HT_MEMALIGN_FOR_STRG?mstore_addstr(ht->keystore,bytes,bytes_l): mstore_add(ht->keystore,bytes,bytes_l,memoryalign));

    //log_debug(ANSI_MAGENTA"ht_internalize mstore_add %ld " ANSI_RESET " bytes: %p mstore_contains:%d\n",bytes_l,bytes,mstore_contains(ht->keystore,e->key));
    //    ASSERT(e->value==ht_intern(ht,e->value,bytes_l,0,memoryalign));
  }//else log_debug(ANSI_GREEN"ht_internalize %ld "ANSI_RESET,bytes_l);

  return e->value;
}
/**************************************** */
/* ***  Use struct ht to avoid dups   *** */
/**************************************** */
static bool ht_only_once(struct ht *ht,const char *s,int s_l){
  if (!s) return false;
  if (!ht) return true;
  //ASSERT(!(ht->flags&HT_FLAG_NUMKEY)); // DEBUG_NOW
  if (!s_l) s_l=strlen(s);
  return !ht_set(ht,s,s_l,0,"");
}

/* static bool ht_numkey_only_once(struct ht *ht,const char *s,int s_l){ */
/*   if (!s) return false; */
/*   if (!ht) return true; */
/*   if (!s_l) s_l=strlen(s); */
/*   return 0!=(ht->flags&HT_FLAG_NUMKEY)? */
/*     !ht_numkey_set(ht,hash64(s,s_l),hash32_java(s,s_l)|(((long)s_l)<<32),""): /\* Accept very rare collision *\/ */
/*     !ht_set(ht,s,s_l,0,""); */
/* } */


/* No need for strlen */
/***************************************************************** */
/* ***  Less parameters.  No need for strlen of key.           *** */
/***************************************************************** */
static void* ht_sget(struct ht *ht, const char* key){
  return !key?NULL:ht_get(ht,key,strlen(key),0);
}
static struct ht_entry* ht_sget_entry(struct ht *ht, const char* key,const bool create){
  return !key?NULL:ht_get_entry(ht,key,strlen(key),0,create);
}
static void *ht_sset(struct ht *ht,const char* key, const void* value){
  return !key?NULL:ht_set(ht,key,strlen(key),0,value);
}
const void *ht_sinternalize(struct ht *ht,const char *key){
  return !key?NULL:ht_intern(ht,key,strlen(key),0,HT_MEMALIGN_FOR_STRG);
}



/***************************************************************** */
/* ***  Functions with prefix ht_numkey_:                      *** */
/* ***  A 64 bit value is assigned to two 64 bit numeric keys  *** */
/* ***  key1 needs to have high variability                    *** */
/***************************************************************** */
static struct ht_entry *ht_numkey_get_entry(struct ht *ht, uint64_t key_high_variability, uint64_t const key2,bool create){
#define E() _ht_get_entry_maybe_empty(ht,(char*)(uint64_t)(key2),(key_high_variability));
  ASSERT(0!=(ht->flags&HT_FLAG_NUMKEY));
  CG_THREAD_OBJECT_ASSERT_LOCK(ht);
  ASSERT(ht->entries!=NULL);
  if (!key_high_variability && !key2) return &ht->entry_zero;
  struct ht_entry *e=E();
  if (!e->key && !e->keylen_hash && create){
    if (_ht_expand(ht)==1) e=E();
    e->key=(char*)key2;
    e->keylen_hash=key_high_variability;
    ht->length++;
  }
  return e;
#undef E
}
#define ht_numkey_get(ht,key_high_variability,key2)  ht_numkey_get_entry(ht,key_high_variability,key2,false)->value

static void *ht_numkey_set(struct ht *ht, uint64_t key_high_variability, const uint64_t key2, const void *value){
  struct ht_entry *e=ht_numkey_get_entry(ht,key_high_variability,key2,true);
  void *old=e->value;
  e->value=(void*)value;
  return old;
}


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
      if (hash_value_strg(k)!=(ee[i].keylen_hash&UINT32_MAX)){ printf("HHHHHHHHHHHHHHHHHHHHHHHH!\n");EXIT(1);}
    }
  }
}
static void test_ht_1(int argc,char *argv[]){
  struct ht ht;
  ht_init_with_keystore_dim(&ht,7,65536);
  // ht_init(&ht,0,7);
  const char *VV[]={"A","B","C","D","E","F","G","H","I"};
  const int L=9;
  FOR(i,1,argc){
    ht_sset(&ht,argv[i],VV[i%L]);
  }
  //  debug_print_keys(&ht);
  FOR(i,1,argc){
    char *a=argv[i],*fetched=ht_sget(&ht,a);
    printf("argv[%2d]: %s  fetched: %s  VV[%d]: %s\n",i,a,fetched,i%L,VV[i%L]);
    if (!fetched){printf(" !!!! fetched is NULL\n"); EXIT(9);}
    assert(!strcmp(fetched,VV[i%L]));
  }
  ht_destroy(&ht);
  printf(" ht.length: %u\n",ht.length);
}
static void test_num_keys(int argc, char *argv[]){
  if(argc!=2){
    printf("Expected single argument. A decimal number denoting number of tests\n");
    return;
  }
  const int n=atoi(argv[1]);
  struct ht ht;
  ht_init(&ht,HT_FLAG_NUMKEY|9);
  printf(ANSI_UNDERLINE""ANSI_BOLD"Testing 0 keys"ANSI_RESET"\n");
  printf("ht_numkey_get(&ht,0,0): %p  ht.length: %u\n",ht_numkey_get(&ht,0,0),ht.length);
  ht_numkey_set(&ht,0,0,(void*)7);
  printf("After setting 7 ht_numkey_get(&ht,0,0): %p  ht.length: %u\n\n",ht_numkey_get(&ht,0,0),ht.length);
  printf(ANSI_INVERSE""ANSI_BOLD"i\tj\tu\tv"ANSI_RESET"\n");
  int length=0;
  RLOOP(set,2){
    FOR(i,0,n){
      FOR(j,0,n){
        const int u=i*i+j*j;
        if (set){
          ht_numkey_set(&ht,i,j,(void*)(uint64_t)u);
          if (i || j) length++;
          //          assert(u==(uint64_t)ht_numkey_get(&ht,i,j));
        }else{
          const uint64_t v=(uint64_t)ht_numkey_get(&ht,i,j);
          printf("%s%d\t%d\t%d\t%"PRIu64"\n"ANSI_RESET,   u!=v?ANSI_FG_RED:ANSI_FG_GREEN, i,j,u,v);
          if (u!=v) goto u_ne_v;
        }
      }
    }
  }
 u_ne_v:
  printf("ht.length: %u   length: %d\n",ht.length,length);
  assert(ht.length==length);
  ht_destroy(&ht);
}
static void test_internalize(int argc, char *argv[]){
  mstore_set_base_path("~/tmp/cache/test_internalize");
  struct ht ht;
  ht_init_interner_file(&ht,"test_internalize",0,8,4096);
  FOR(i,0,argc){
    const char *s=argv[i];
    assert(ht_sinternalize(&ht,s)==ht_sinternalize(&ht,s));
    assert(ht_sinternalize(&ht,s)==ht_sget(&ht,s));
    FOR(j,0,3){
      char *isIntern=mstore_contains(ht.keystore,s)?"Yes":"No";
      printf("%d\t%d\t%s\t%p\t%p\t%s\n",i,j,s,(void*)s,ht_sget(&ht,s),isIntern);
      s=ht_sinternalize(&ht,s);
    }
    printf("\n");
  }
  printf("ht->length: %u\n",ht.length);
  ht_destroy(&ht);
}


static void test_intern_num(int argc, char *argv[]){
  struct ht ht;
  #if 0
  struct mstore m={0};  mstore_init(&m,4096);  ht_init_with_keystore(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|2,&m);
  #else
  ht_init_interner(&ht,2,4096);
  #endif
  const int N=atoi(argv[1]);
  int data[N][10];
  int *intern[N];
  FOR(i,0,N)  FOR(j,0,5)  data[i][j]=rand();
  const int data_l=10*sizeof(int);
  const int SINT=sizeof(int);
  bool verbose=false;
  FOR(i,0,N) intern[i]=(int*)ht_intern(&ht,data[i],data_l,0,SINT);
  int count_ok=0;
  FOR(i,0,N){
    bool  ok;
    ok=(intern[i]==(int*)ht_intern(&ht,intern[i],data_l,0,SINT));

    //      assert(hash32(intern[i],10*SINT)==hash32(data[i]));;

    assert(ok); if (verbose) log_verbose("ok2 %d ",ok);

    ok=(intern[i]==(int*)ht_intern(&ht,data[i],data_l,0,SINT));
    assert(ok); if (verbose) log_verbose("ok1 %d ",ok);
    ok=(intern[i]==(int*)ht_get(&ht,(char*)intern[i],data_l,0));
    assert(ok); if (verbose) log_verbose("ok3 %d ",ok);
    ok=(intern[i]==(int*)ht_get(&ht,(char*)data[i],data_l,0));
    assert(ok); if (verbose) log_verbose("ok3 %d ",ok);
    if (verbose) putchar('\n');
    if (ok) count_ok++;
  }
  ht_destroy(&ht);
  printf("count_ok=%d ht.length: %u \n",count_ok,ht.length);
}


static void test_use_as_set(int argc, char *argv[]){
  struct ht ht;
  ht_init(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|8);
  printf("\x1B[1m\x1B[4m");
  printf("j\ti\ts\told\tSize\n"ANSI_RESET);
  FOR(j,0,5){
    FOR(i,1,argc){
      char *s=argv[i], *old=ht_sset(&ht,s,"x");
      printf("%d\t%d\t%s\t%s\t%u\n",j,i,s,old,ht.length);
    }
    fputc('\n',stderr);
  }
  printf("ht.length: %u \n",ht.length);
}
static void test_unique(int argc, char *argv[]){
  struct ht ht;
  ht_init_with_keystore_dim(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|8,4096);
  for(int i=1;i<argc && i<999;i++){
    if (ht_only_once(&ht,argv[i],0)) printf("%s\t",argv[i]);
  }
  printf("\n ht.length: %u \n",ht.length);
}

#include "cg_utils.c"
static void test_mstore2(int argc,char *argv[]){
  mstore_set_base_path("/home/cgille/tmp/test/mstore_mstore1");
  struct ht ht_int,ht;
  struct mstore m;
  mstore_init(&m,1024*1024*1024);
  ht_init_with_keystore(&ht_int,HT_FLAG_KEYS_ARE_STORED_EXTERN|8,&m);
  ht_init(&ht,16);
  const int nLine=argc>2?atoi(argv[2]):INT_MAX;
  char value[99];
  size_t len=999;
  off_t n;
  char *line=malloc(len);
  FOR(pass,0,2){
    printf("\n pass=%d\n\n",pass);
    FILE *f=fopen(argv[1],"r");  assert(f!=NULL);
    for(int iLine=0;(n=getline(&line,&len,f))!=-1 && iLine<nLine;iLine++){
      const int line_l=strlen(line)-1;
      line[line_l]=0;
      sprintf(value,"%x",hash32(line,line_l));
      if (pass==0){
        const int value_l=strlen(value);
        //fputs("(5",stderr);
        char *key;
        {
          char line_on_stack[line_l+1];
          strcpy(line_on_stack,line);
          key=(char*)ht_intern(&ht_int,line_on_stack,line_l,0,HT_MEMALIGN_FOR_STRG);
        }/* From here line_on_stack invalid */
        //fputs("5)",stderr);
        const int key_l=strlen(key);
        const char *stored=mstore_addstr(&m,value,value_l);
        //fputc(',',stderr);

        ht_set(&ht,key,key_l,0,stored);

        //printf(" l:%d ",ht.length);
        //printf("key=%s  %s\n",key,ht_get(&ht,key,key_l,0));
        if (is_square_number(iLine))          printf(" %d ",iLine);
      }else{
        const char *from_cache=ht_sget(&ht,line);
        if (is_square_number(iLine))  printf("(%4d) Line: %s  Length: %jd   hash: %s from_cache: %s \n",iLine, line,(intmax_t)n,value,from_cache);
        assert(!strcmp(value,from_cache));
      }
    }
    fclose(f);
  }
  free(line);
  ht_destroy(&ht);
  ht_destroy(&ht_int);
  printf("\n ht.length: %u \n",ht.length);
}

static void test_intern_substring(int argc,char *argv[]){
  struct ht ht;
  ht_init_with_keystore_dim(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|8,4096);
  FOR(i,1,argc){
    const char *a=argv[i];
    const int len2=strlen(a)/2;
    const char *internalized=ht_intern(&ht,a,len2,0,HT_MEMALIGN_FOR_STRG);
    printf("a: %s len/2: %d internalized: %s\n",a,len2,internalized);
  }
  printf("\n ht.length: %u \n",ht.length);
}

static  void test_no_dups(int argc,char *argv[]){
  struct ht no_dups={0};
  ht_init_with_keystore_dim(&no_dups,8,4096);
  FOR(i,1,argc) if (ht_only_once(&no_dups,argv[i],0)) printf("%d %s ,ht.length: %u\n",i,argv[i],no_dups.length);
}

int main(int argc,char *argv[]){
  if (0){
    struct ht ht={0};
    ht_init_interner(&ht,8,4096);
    const char *key=ht_sinternalize(&ht,"key");
    printf("key: %s contains:%d  ht.length: %u\n",key,mstore_contains(ht.keystore,key),ht.length);
    return 1;
  }

  switch(5){
  case 0:
    printf("ht_entry %zu bytes\n",sizeof(struct ht_entry));
    assert(false);break;
  case 2: test_no_dups(argc,argv);break; /* a b c d */
  case 3: test_unique(argc,argv);break; /* a a a a b b b c c  */
  case 4: test_num_keys(argc,argv);break; /* 30 */
  case 5: test_intern_num(argc,argv);break; /* 100 */
  case 6: test_ht_1(argc,argv);break; /* $(seq 30) */
  case 7: test_internalize(argc,argv);break; /* $(seq 30) */

  case 8: test_use_as_set(argc,argv);break; /* $(seq 30) */
    //  case 5: test_iteration(argc,argv);break; /* {1,2,3,4}{a,b,c,d}{x,y,z}  */

  case 10: test_mstore2(argc,argv); break; /*  f=~/tmp/lines.txt ; seq 100 > $f; cg_ht_v7 $f */
  case 11: test_intern_substring(argc,argv); break; /* abcdefghij   ABCD */
  }
}
#endif
