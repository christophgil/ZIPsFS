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
#define _GNU_SOURCE
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
#include <malloc.h>
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
  struct ht_entry* entries,_stack_ht_entry[_STACK_HT_ENTRY];
  struct mstore keystore_buf, *keystore, *value_store;
#if defined CG_THREAD_FIELDS
  CG_THREAD_FIELDS;
#endif
};
#if defined CG_THREAD_METHODS_HT
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
    if (mstore_dim&MSTORE_OPT_MMAP_WITH_FILE) assert(name!=NULL); /* Needed for file */
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


static MAYBE_INLINE int debug_count_empty(struct ht_entry *ee, const uint32_t capacity){
  int c=0;
  RLOOP(i,capacity) if (HT_ENTRY_IS_EMPTY(ee+i)) c++;
  return c;
}
#define ht_get_entry_common(ht,key,keylen_hash) _ht_get_entry((ht)->entries,(ht)->capacity,0!=((ht)->flags&HT_FLAG_NUMKEY),(key),(keylen_hash))
#define let_e_get_entry(ht,key,keylen_hash) CG_THREAD_OBJECT_ASSERT_LOCK(ht);_ht_expand(ht);struct ht_entry *e=ht_get_entry_common(ht,key,keylen_hash)
static struct ht_entry* _ht_get_entry(struct ht_entry *ee, const uint32_t capacity, const bool intkey,const char* key, const uint64_t keylen_hash){
  struct ht_entry *e=ee+(keylen_hash&(capacity-1));
#define _HT_AT_END_OF_ENTRIES_WRAP_AROUND() if (++e>=ee+capacity){e=ee;assert(!count_wrap_around++);}
#define _HT_LOOP_TILL_EMPTY_ENTRY(cond)  int count_wrap_around=0;while (e->key cond)
  if (intkey){
    _HT_LOOP_TILL_EMPTY_ENTRY(|| e->keylen_hash || e->value){
      if (e->keylen_hash==keylen_hash && key==e->key) break;
      _HT_AT_END_OF_ENTRIES_WRAP_AROUND();
    }
  }else{
    assert(key!=NULL);
    _HT_LOOP_TILL_EMPTY_ENTRY(){
      if (e->key && e->keylen_hash==keylen_hash && (key==e->key || !memcmp(e->key,key,keylen_hash>>HT_KEYLEN_SHIFT))) break;
      _HT_AT_END_OF_ENTRIES_WRAP_AROUND();
    }
  }
#undef _HT_LOOP_TILL_EMPTY_ENTRY
#undef _HT_AT_END_OF_ENTRIES_WRAP_AROUND
  return e;
}
static const char* _newKey(struct ht *ht,const char *key,uint64_t keylen_hash){
  if (0!=(ht->flags&HT_FLAG_KEYS_ARE_STORED_EXTERN)) return key;
  return ht->keystore?mstore_addstr(ht->keystore,key,keylen_hash>>HT_KEYLEN_SHIFT):strdup(key);
}
static bool _ht_expand(struct ht *ht);
static struct ht_entry* ht_get_entry(struct ht *ht, const char* key,const ht_keylen_t key_l,ht_hash_t hash,const bool create){
  _HT_HASH();
  assert(key!=NULL);
  const uint64_t keylen_hash=HT_KEYLEN_HASH(key_l,hash);
  let_e_get_entry(ht,key,keylen_hash);
  if (create && HT_ENTRY_IS_EMPTY(e)){
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
  _HT_HASH();
  const uint64_t keylen_hash=HT_KEYLEN_HASH(key_l,hash);
  let_e_get_entry(ht,key,keylen_hash);
  if (e->key || e->keylen_hash==keylen_hash){
    if (!ht->keystore) free((char*)e->key);
    ht_clear_entry(e);
    ht->length--;
  }
  return e;
}
static void *ht_set_common(struct ht *ht,const char* key,uint64_t keylen_hash, const void* value){
  CG_THREAD_OBJECT_ASSERT_LOCK(ht);
  assert(ht->entries!=NULL);
  let_e_get_entry(ht,key,keylen_hash);
  if (!e->key && !e->keylen_hash){ /* Didn't find key, allocate+copy if needed, then insert it. */
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
static void *ht_set(struct ht *ht,const char* key,const ht_keylen_t key_l,ht_hash_t hash, const void* value){
  _HT_HASH();
  assert(key!=NULL);
  return ht_set_common(ht,key,HT_KEYLEN_HASH(key_l,hash),value);
}

/*
  Both, key_l and hash can be  zero. In this case they are computed.
*/
static void* ht_get(struct ht *ht, const char* key,const ht_keylen_t key_l,ht_hash_t hash){
  _HT_HASH();
  CG_THREAD_OBJECT_ASSERT_LOCK(ht);
  if (!key) return NULL;
  assert(ht!=NULL && ht->entries!=NULL);
  const struct ht_entry *e=ht_get_entry(ht,key,key_l,hash,false);
  return e->key?e->value:NULL;
}
/*
  Expand hash ht to twice its current size. Return true on success,
  Return false if out of memory.
*/
static bool _ht_expand(struct ht *ht){
  if (ht->length<(ht->capacity>>1)) return true;
  const uint32_t new_capacity=ht->capacity<<1;
  if (new_capacity<ht->capacity) return false;  /* overflow (capacity would be too big) */
  struct ht_entry* new_ee=calloc(new_capacity,sizeof(struct ht_entry));
  if (!new_ee){ log_error0("ht.c: calloc new_ee\n"); return false; }
  for(uint32_t i=0; i<ht->capacity; i++){
    const struct ht_entry *e=ht->entries+i;
    if (e->key || e->keylen_hash){
      *_ht_get_entry(new_ee,new_capacity,0!=(ht->flags&HT_FLAG_NUMKEY),e->key,e->keylen_hash)=*e;
    }
  }
  _ht_free_entries(ht);
  ht->entries=new_ee;
  ht->capacity=new_capacity;
  return true;
}
/***************************************************************** */
/* ***  A 64 bit value is assigned to two 64 bit numeric keys  *** */
/* ***  key1 needs to have high variability                    *** */
/***************************************************************** */
#define HT_SET_INT_PREAMBLE() assert(0!=(ht->flags&HT_FLAG_NUMKEY))
static void *ht_numkey_set(struct ht *ht, uint64_t key_high_variability, const uint64_t key2, const void *value){
  HT_SET_INT_PREAMBLE();
  return ht_set_common(ht,(char*)key2,key_high_variability,value);
}

#define HT_ENTRY_SET_NUMKEY(e,key_high_variability,key2)   {e->key=(char*)(uint64_t)key2;e->keylen_hash=key_high_variability;}
#define ht_numkey_get_entry(ht,key_high_variability,key2) ht_get_entry_common(ht,(char*)(uint64_t)(key2),(key_high_variability))
static void *ht_numkey_get(struct ht *ht, uint64_t key_high_variability, uint64_t const key2){
  HT_SET_INT_PREAMBLE();
  let_e_get_entry(ht,(char*)key2,key_high_variability);
  return e->value;
}
#undef HT_SET_INT_PREAMBLE
static void ht_clear(struct ht *ht){
  CG_THREAD_OBJECT_ASSERT_LOCK(ht);
  struct ht_entry *e=ht->entries;
  if (e){
    memset(e,0,ht->capacity*sizeof(struct ht_entry));
mstore_clear(ht->keystore);
  }
}
/* memoryalign is  [ 1,2,4,8] or 0 forString
See https://docs.rs/string-interner/latest/string_interner/
https://en.wikipedia.org/wiki/String_interning
 */
const void *ht_intern(struct ht *ht,const void *bytes,const size_t bytes_l,ht_hash_t hash,const int memoryalign){
  assert(ht->keystore!=NULL);
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

static bool ht_only_once(struct ht *ht,const char *s,int s_l){
  if (!s) return false;
  if (!ht) return true;
  if (!s_l) s_l=strlen(s);
  return 0!=(ht->flags&HT_FLAG_NUMKEY)?
    !ht_numkey_set(ht,hash64(s,s_l),hash32_java(s,s_l)|(((long)s_l)<<32),""): /* Accept very rare collision */
    !ht_set(ht,s,s_l,0,"");
}

/* No need for strlen */
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


#endif
#define _ht_dot_c_end
// 1111111111111111111111111111111111111111111111111111111111111
#if __INCLUDE_LEVEL__ == 0
static void debug_print_keys(struct ht *ht){
  const struct ht_entry *ee=ht->entries;
  for(uint32_t i=0;i<ht->capacity;i++){
    const char *k=ee[i].key;
    if (k){
      fprintf(stderr,"debug_print_keys %u\t%s\n",i,k);
      if (hash_value_strg(k)!=(ee[i].keylen_hash&UINT32_MAX)){ fprintf(stderr,"HHHHHHHHHHHHHHHHHHHHHHHH!\n");EXIT(1);}
    }
  }
}
static void test_ht_1(int argc,  char *argv[]){
  struct ht ht;
  ht_init_with_keystore_dim(&ht,7,65536);
  // ht_init(&ht,0,7);
  char *VV[]={"A","B","C","D","E","F","G","H","I"};
  const int L=9;
  for(int i=1;i<argc;i++){
    ht_sset(&ht,argv[i],VV[i%L]);
  }
  //  debug_print_keys(&ht);
  for(int i=1;i<argc;i++){
    char *fetched=(char*)ht_sget(&ht,argv[i]);
    fprintf(stderr,"argv[i]=%s  s=%s\n",argv[i],fetched);
    if (!fetched){fprintf(stderr," !!!! fetched is NULL\n"); EXIT(9);}
    assert(!strcmp(fetched,VV[i%L]));
  }
  ht_destroy(&ht);
}
static void test_int_keys(int argc, char *argv[]){
  if(argc!=2){
    fprintf(stderr,"Expected single argument. A decimal number denoting number of tests\n");
    return;
  }
  const int n=atoi(argv[1]);
  struct ht ht;
  ht_init(&ht,HT_FLAG_NUMKEY|9);
  fprintf(stderr,ANSI_UNDERLINE""ANSI_BOLD"Testing 0 keys"ANSI_RESET"\n");
  fprintf(stderr,"ht_numkey_get(&ht,0,0): %p\n",ht_numkey_get(&ht,0,0));
  ht_numkey_set(&ht,0,0,(void*)7);
  fprintf(stderr,"After setting 7 ht_numkey_get(&ht,0,0): %p\n\n",ht_numkey_get(&ht,0,0));
  fprintf(stderr,ANSI_INVERSE""ANSI_BOLD"i\tj\tu\tv"ANSI_RESET"\n");
  RLOOP(set,2){
    FOR(i,0,n){
      FOR(j,0,n){
        const int u=i*i+j*j;
        if (set){
          ht_numkey_set(&ht,i,j,(void*)(uint64_t)u);
          //          assert(u==(uint64_t)ht_numkey_get(&ht,i,j));
        }else{
          const uint64_t v=(uint64_t)ht_numkey_get(&ht,i,j);
          fprintf(stderr,"%s%d\t%d\t%d\t%"PRIu64"\n"ANSI_RESET,   u!=v?ANSI_FG_RED:ANSI_FG_GREEN, i,j,u,v);
          if (u!=v) goto u_ne_v;
        }
      }
    }
  }
 u_ne_v:
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
  ht_destroy(&ht);
}


static void test_intern_num(int argc, char *argv[]){
  struct ht ht;
  struct mstore m={0};
  mstore_init(&m,4096);
  ht_init_with_keystore(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|2,&m);
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
  printf("count_ok=%d\n",count_ok);
}


static void test_use_as_set(int argc, char *argv[]){
  struct ht ht;
  ht_init(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|8);
  fprintf(stderr,"\x1B[1m\x1B[4m");
  fprintf(stderr,"j\ti\told\ts\tSize\n"ANSI_RESET);
  FOR(j,0,5){
    FOR(i,1,argc){
      char *s=argv[i], *old=ht_sset(&ht,s,"x");
      fprintf(stderr,"%d\t%d\t%s\t%s\t%u\n",j,i,s,old,ht.length);
    }
    fputc('\n',stderr);
  }
}
static void test_unique(int argc, char *argv[]){
  struct ht ht;
  ht_init_with_keystore_dim(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|8,4096);
  for(int i=1;i<argc && i<999;i++){
    if (ht_only_once(&ht,argv[i],0)) fprintf(stderr,"%s\t",argv[i]);
  }
  fprintf(stderr,"\n");
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
  size_t len=999,n;
  char *line=malloc(len);
  FOR(pass,0,2){
    fprintf(stderr,"\n pass=%d\n\n",pass);
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

        //fprintf(stderr," l:%d ",ht.length);
        //printf("key=%s  %s\n",key,ht_get(&ht,key,key_l,0));
        if (is_square_number(iLine))          fprintf(stderr," %d ",iLine);
      }else{
        const char *from_cache=ht_sget(&ht,line);
        if (is_square_number(iLine))  fprintf(stderr,"(%4d) Line: %s  Length: %zu   hash: %s from_cache: %s \n",iLine, line,n,value,from_cache);
        assert(!strcmp(value,from_cache));
      }
    }
    fclose(f);
  }
  free(line);
  ht_destroy(&ht);
  ht_destroy(&ht_int);
}

static void test_intern_substring(int argc,char *argv[]){
  struct ht ht;
  ht_init_with_keystore_dim(&ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|8,4096);

  const char *internalized=ht_intern(&ht,"abc",5,0,HT_MEMALIGN_FOR_STRG);
  printf("internalized=%s\n",internalized);
}
int main(int argc,char *argv[]){
  if (0){
  struct ht ht={0};
  ht_init_interner(&ht,8,4096);

  const char *key=ht_sinternalize(&ht,"key");
  printf("key: %s contains:%d\n",key,mstore_contains(ht.keystore,key));
  return 1;
  }

  switch(5){
  case 0:
    fprintf(stderr,"ht_entry %zu bytes\n",sizeof(struct ht_entry));
    assert(false);break;
  case 1: test_int_keys(argc,argv);break; /* 30 */
  case 2: test_ht_1(argc,argv);break; /* $(seq 30) */
  case 3: test_internalize(argc,argv);break; /* $(seq 30) */
  case 4: test_intern_num(argc,argv);break; /* 100 */
  case 5: test_use_as_set(argc,argv);break; /* $(seq 30) */
    //  case 5: test_iteration(argc,argv);break; /* {1,2,3,4}{a,b,c,d}{x,y,z}  */
  case 7: test_unique(argc,argv);break; /* a a a a b b b c c  */
  case 8: test_mstore2(argc,argv); break; /*  f=~/tmp/lines.txt ; seq 100 > $f; cg_ht_v7 $f */
  case 9: test_intern_substring(argc,argv); break;
  }
}
#endif
