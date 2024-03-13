#ifndef _cg_crc32_dot_c
#define _cg_crc32_dot_c

#include <inttypes.h>
#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>


static uint32_t crc32_for_byte(uint32_t r){
  for(int j=0; j<8; ++j) r=((r&1)?0:(uint32_t)0xEDB88320L)^r>>1;
  return r^(uint32_t)0xFF000000L;
}
typedef uint64_t accum_t; /* Or uint32_t */
static void cg_crc32_init_tables(uint32_t* table, uint32_t* wtable){
  for(size_t i=0; i<0x100; ++i) table[i]=crc32_for_byte(i);
  for(size_t k=0; k<sizeof(accum_t); ++k){
    for(size_t w,i=0; i<0x100; ++i){
      for(size_t j=w=0; j<sizeof(accum_t); ++j)
        w=table[(uint8_t)(j==k?w^i:w)]^w>>8;
      wtable[(k<<8)+i]=w^(k?wtable[0]:0);
    }
  }
}

static uint32_t cg_crc32(const void *data, size_t n_bytes, uint32_t crc, pthread_mutex_t *mutex){
  //assert( ((uint64_t)data)%sizeof(accum_t)==0); /* Assume alignment at 16 bytes --  No not true*/
  static uint32_t table[0x100]={0}, wtable[0x100*sizeof(accum_t)];
  static volatile bool initialized=false;
  const size_t n_accum=n_bytes/sizeof(accum_t);
  if (!initialized){
    if (mutex) pthread_mutex_lock(mutex);
    cg_crc32_init_tables(table,wtable);
    initialized=true;
    if (mutex) pthread_mutex_unlock(mutex);
  }
  for(size_t i=0; i<n_accum; ++i){
    const accum_t a=crc^((accum_t*)data)[i];
#define C(j) (wtable[(j<<8)+(uint8_t)(a>>8*j)])
    if (sizeof(accum_t)==8){
      crc=C(0)^C(1)^C(2)^C(3)^C(4)^C(5)^C(6)^C(7);
    }else{
      for(int j=crc=0; j<sizeof(accum_t); ++j) crc^=C(j);
    }
#undef C
  }
  for(size_t i=n_accum*sizeof(accum_t);i<n_bytes;++i) crc=table[(uint8_t)crc^((uint8_t*)data)[i]]^crc>>8;
  return crc;
}
#endif // _cg_crc32_dot_c
#if defined __INCLUDE_LEVEL__ && __INCLUDE_LEVEL__ == 0
int main(int argc, char *argv[]){

}
#endif
