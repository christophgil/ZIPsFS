/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
#ifndef _cg_utils_dot_c
#define _cg_utils_dot_c
#define _GNU_SOURCE
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <linux/limits.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/statfs.h>
#include <locale.h>
#include "cg_log.c"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MAX_PATHLEN 512
#define DEBUG_NOW 1
#define FREE(s) free((void*)s),s=NULL
/*********************************************************************************/
/* *** String *** */
static int empty_dot_dotdot(const char *s){ return !*s || (*s=='.' && (!s[1] || (s[1]=='.' && !s[2]))); }
static char *my_strncpy(char *dst,const char *src, int n){
  *dst=0;
  if (src) strncat(dst,src,n);
  return dst;
}
#define SNPRINTF(dest,max,...)   (max<=snprintf(dest,max,__VA_ARGS__) && (log_error("Exceed snprintf "),true))
static unsigned int my_strlen(const char *s){ return !s?0:strnlen(s,MAX_PATHLEN);}
static const char* snull(const char *s){ return s?s:"Null";}
static inline char *yes_no(int i){ return i?"Yes":"No";}

#define ENDSWITH(s,slen,ending)  ((slen>=((int)sizeof(ending)-1) && (!memcmp(s+slen-((int)sizeof(ending)-1),ending,(int)sizeof(ending)))))
#define ENDSWITHI(s,slen,ending) ((slen>=((int)sizeof(ending)-1) && (!strcasecmp(s+slen-((int)sizeof(ending)-1),ending))))

static bool endsWith(const char* s,const char* e){
  if (!s || !e) return false;
  const int sn=strlen(s),en=strlen(e);
  return en<=sn && 0==strcmp(s+sn-en,e);
}
static int count_slash(const char *p){
  const int n=my_strlen(p);
  int count=0;
  for(int i=n;--i>=0;) if (p[i]=='/') count++;
  return count;
}
static int last_slash(const char *path){
  for(int i=my_strlen(path);--i>=0;){
    if (path[i]=='/') return i;
  }
  return -1;
}
static int slash_not_trailing(const char *path){
  char *p=strchr(path,'/');
  return p && p[1]?(int)(p-path):-1;
}
static int pathlen_ignore_trailing_slash(const char *p){
  const int n=my_strlen(p);
  return n && p[n-1]=='/'?n-1:n;
}


static int path_for_fd(const char *title, char *path, int fd){
  *path=0;
  char buf[99];
  sprintf(buf,"/proc/%d/fd/%d",getpid(),fd);
  const ssize_t n=readlink(buf,path, MAX_PATHLEN-1);
  if (n<0){
    log_error("\n%s  %s: path_for_fd ",snull(title),buf);
    perror(" ");
    return -1;
  }
  return path[n]=0;
}


static bool check_path_for_fd(const char *title, const char *path, int fd){
  char check_path[MAX_PATHLEN],rp[PATH_MAX];
  if (!realpath(path,rp)){
    log_error("check_path_for_fd: %s  Failed realpath(%s)\n",snull(title),path);
    return false;
  }
  path_for_fd(title,check_path,fd);
  if (strncmp(rp,path,MAX_PATHLEN-1)){
    log_error("check_path_for_fd %s  fd=%d,%s   d->path=%s   realpath(path)=%s\n",title,fd,check_path,path,rp);
    return false;
  }
  return true;
}

static void print_path_for_fd(int fd){
  char buf[99],path[512];
  sprintf(buf,"/proc/%d/fd/%d",getpid(),fd);
  const ssize_t n=readlink(buf,path,511);
  if (n<0){
    printf(ANSI_FG_RED"Warning %s  No path\n"ANSI_RESET,buf);
    perror(" ");
  }else{
    path[n]=0;
    printf("Path for %d:  %s\n",fd,path);
  }
}




static int min_int(int a,int b){ return MIN(a,b);}
static int max_int(int a,int b){ return MAX(a,b);}
static long min_long(long a,long b){ return MIN(a,b);}
static long max_long(long a,long b){ return MAX(a,b);}
static long atol_kmgt(const char *s){
  if (!s) return 0;
  char *c=(char*)s;
  while(*c && '0'<=*c && *c<='9') c++;
  *c&=~32;
  return atol(s)<<(*c=='K'?10:*c=='M'?20:*c=='G'?30:*c=='T'?40:0);
}
static void log_file_mode(mode_t m){
  char mode[11];
  int i=0;
  mode[i++]=S_ISDIR(m)?'d':'-';
#define C(m,f) mode[i++]=(m&S_IRUSR)?f:'-';
  C(S_IRUSR,'r');C(S_IWUSR,'w');C(S_IXUSR,'x');
  C(S_IRGRP,'r');C(S_IWGRP,'w');C(S_IXGRP,'x');
  C(S_IROTH,'r');C(S_IWOTH,'w');C(S_IXOTH,'x');
#undef C
  log_strg(mode);
}

static void log_file_stat(const char * name,const struct stat *s){
  char *color=ANSI_FG_BLUE;
#if defined SHIFT_INODE_ROOT
  if (s->st_ino>(1L<<SHIFT_INODE_ROOT)) color=ANSI_FG_MAGENTA;
#endif
  log_msg("%s  size=%lu blksize=%lu blocks=%lu links=%lu inode=%s%lu"ANSI_RESET" dir=%s uid=%u gid=%u ",name,s->st_size,s->st_blksize,s->st_blocks,   s->st_nlink,color,s->st_ino,  yes_no(S_ISDIR(s->st_mode)), s->st_uid,s->st_gid);
  //st_blksize st_blocks f_bsize
  log_file_mode(s->st_mode);
  log_strg("\n");
}



static void log_open_flags(int flags){
  log_msg("flags=%x{",flags);
#define C(a) if (flags&a) log_msg(#a" ")
  C(O_RDONLY); C(O_WRONLY);C(O_RDWR);C(O_APPEND);C(O_ASYNC);C(O_CLOEXEC);C(O_CREAT);C(O_DIRECT);C(O_DIRECTORY);C(O_DSYNC);C(O_EXCL);C(O_LARGEFILE);C(O_NOATIME);C(O_NOCTTY);C(O_NOFOLLOW);C(O_NONBLOCK);C(O_NDELAY);C(O_PATH);C(O_SYNC);C(O_TMPFILE);C(O_TRUNC);
#undef C
  log_strg("}");
}

static void clear_stat(struct stat *st){ if(st) memset(st,0,sizeof(struct stat));}
static long time_ms(){
  struct timeval tp;
  gettimeofday(&tp,NULL);
  return tp.tv_sec*1000+tp.tv_usec/1000;
}
static long file_mtime(const char *f){
  struct stat st={0};
  return stat(f,&st)?0:st.st_mtime;
}
static bool stat_differ(const char *title,struct stat *s1,struct stat *s2){
  if (!s1||!s2) return false; // memcmp would lead to false positives
  char *wrong=NULL;
#define C(f) (wrong=#f,s1->f!=s2->f)
  if (C(st_ino)||C(st_mode)||C(st_uid)||C(st_gid)||C(st_size)||C(st_blksize)||C(st_blocks)||C(st_mtime)||C(st_ctime)||(wrong=NULL,false)){
    log_warn("stat_t.%s\n",wrong);
    log_file_stat(title,s1);
    log_file_stat(title,s2);
    return true;
  }
#undef C
  //  log_succes("Stat are identical: %s\n",title);
  return false;
}
static bool is_dir(const char *f){
  struct stat st={0};
  return !lstat(f,&st) && S_ISDIR(st.st_mode);
}

static int is_regular_file(const char *path){
  struct stat path_stat;
  stat(path,&path_stat);
  return S_ISREG(path_stat.st_mode);
}
bool access_from_stat(struct stat *stats,int mode){ // equivaletn to access(path,mode)
  int granted;
  mode&=(X_OK|W_OK|R_OK);
#if R_OK!=S_IROTH || W_OK!=S_IWOTH || X_OK!=S_IXOTH
  ?error Oops, portability assumptions incorrect.;
#endif
  if (mode==F_OK) return 0;
  if (getuid()==stats->st_uid)
    granted=(unsigned int) (stats->st_mode&(mode<<6))>>6;
  else if (getgid()==stats->st_gid || group_member(stats->st_gid))
    granted=(unsigned int) (stats->st_mode&(mode<<3))>>3;
  else
    granted=(stats->st_mode&mode);
  return granted==mode;
}
/*********************************************************************************/
/* *** io *** */
static void print_substring(int fd,char *s,int f,int t){  write(fd,s,min_int(my_strlen(s),t)); }
static void recursive_mkdir(char *p){
  const int n=pathlen_ignore_trailing_slash(p);
  for(int i=2;i<n;i++){
    if (p[i]=='/') {
      p[i]=0;
      mkdir(p,S_IRWXU);
      p[i]='/';
    }
  }
  mkdir(p,S_IRWXU);
}
// #pragma GCC diagnostic ignored "-Wunused-variable" //__attribute__((unused));
/* **********************************************/
/* *** CRC32 *** */
/* Source:  http://home.thep.lu.se/~bjorn/crc/ */

static uint32_t crc32_for_byte(uint32_t r) {
  for(int j=0; j<8; ++j) r=((r&1)?0:(uint32_t)0xEDB88320L)^r>>1;
  return r^(uint32_t)0xFF000000L;
}
typedef unsigned long accum_t; /* Or unsigned int */
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
static uint32_t cg_crc32(const void *data, size_t n_bytes, uint32_t crc){
  //assert( ((uint64_t)data)%sizeof(accum_t)==0); /* Assume alignment at 16 bytes --  No not true*/
  static uint32_t table[0x100]={0}, wtable[0x100*sizeof(accum_t)];
  const size_t n_accum=n_bytes/sizeof(accum_t);
  if(!*table) cg_crc32_init_tables(table,wtable);
  for(size_t i=0; i<n_accum; ++i){
    const accum_t a=crc^((accum_t*)data)[i];
#define C(j) (wtable[(j<<8)+(uint8_t)(a>>8*j)])
    if (sizeof(accum_t)==8){
      crc=crc^C(0)^C(1)^C(2)^C(3)^C(4)^C(5)^C(6)^C(7);
    }else{
      for(int j=crc=0; j<sizeof(accum_t); ++j) crc^=C(j);
    }
#undef C
  }
  for(size_t i=n_accum*sizeof(accum_t);i<n_bytes;++i) crc=table[(uint8_t)crc^((uint8_t*)data)[i]]^crc>>8;
  return crc;
}







#endif // _cg_utils_dot_c
// 1111111111111111111111111111111111111111111111111111111111111


#if __INCLUDE_LEVEL__ == 0

int main(int argc, char *argv[]){
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in printf */
  //char *s=argv[1];    printf("%s = %'ld\n",s,atol_kmgt(s));
  {
    const char *s=argv[1];
  int l=strlen(s);
  printf("l=%d\n",l);

  printf("strcmp %d\n",ENDSWITH(s,l,".zip"));
  printf("strcmp %d\n",ENDSWITHI(s,l,".zip"));

}
  /*
  const char *path="/home/cgille/tmp/ZIPsFS/modified/PRO1/Data/30-0046/20230126_PRO1_KTT_004_30-0046_LisaKahl_P01_VNATSerAuxpM1evoM2Glucose10mMFormate20mM_dia_BD11_1_12097.d/analysis.tdf";
  //  int res=open(path,8201,509);
    int res=open(path,O_WRONLY|O_TRUNC);
  printf("res=%d path=%s\n",res,path);
  */



  if (0){
    printf("sizeof=%zu\n",sizeof(".txt"));
    printf("ENDSWITH=%d\n",ENDSWITH(argv[1],strlen(argv[1]),".txt"));
  }
  if (0){
    char *a="abcdefghij";
    const uint32_t crc=cg_crc32(a,strlen(a),0);
    printf("%s crc=%x\n",a,crc);
    //    abcdefghij crc=3981703a
    exit(9);
  }
}
#endif
