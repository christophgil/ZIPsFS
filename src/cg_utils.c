/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
#include "cg_utils.h"
#ifndef _cg_utils_dot_c
#define _cg_utils_dot_c
#include <errno.h>
#include <fcntl.h>// provides posix_fadvise
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>
#include <time.h>
#include <utime.h>
#include <grp.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>

#include "cg_gnu.c"

/*********************************************************************************/
static bool has_proc_fs(){
  struct stat st;
  static int v;
  if (!v) v=stat("/proc/self/fd",&st)?-1:1;
  return v==1;
}

//////////////
/// perror ///
//////////////
#define C(x)  case x: return #x
static const char *error_symbol(const int x){
  switch(x){
#include "cg_utils_error_codes.c"
  };
  return "?";
}
#ifdef ZIP_ER_OK
static const char *error_symbol_zip(const int x){
  switch(x){
    C(ZIP_ER_OK); C(ZIP_ER_MULTIDISK); C(ZIP_ER_RENAME); C(ZIP_ER_CLOSE); C(ZIP_ER_SEEK); C(ZIP_ER_READ); C(ZIP_ER_WRITE); C(ZIP_ER_CRC); C(ZIP_ER_ZIPCLOSED); C(ZIP_ER_NOENT);
    C(ZIP_ER_EXISTS); C(ZIP_ER_OPEN); C(ZIP_ER_TMPOPEN); C(ZIP_ER_ZLIB); C(ZIP_ER_MEMORY); C(ZIP_ER_CHANGED);
    C(ZIP_ER_COMPNOTSUPP); C(ZIP_ER_EOF); C(ZIP_ER_INVAL); C(ZIP_ER_NOZIP); C(ZIP_ER_INTERNAL); C(ZIP_ER_INCONS); C(ZIP_ER_REMOVE); C(ZIP_ER_DELETED); C(ZIP_ER_ENCRNOTSUPP); C(ZIP_ER_RDONLY); C(ZIP_ER_NOPASSWD);
    C(ZIP_ER_WRONGPASSWD); C(ZIP_ER_OPNOTSUPP); C(ZIP_ER_INUSE); C(ZIP_ER_TELL); C(ZIP_ER_COMPRESSED_DATA);C(ZIP_ER_CANCELLED);
  };
  return "?";
}
#endif //defined ZIP_ER_OK
#undef C



static void fprint_strerror(FILE *f,int err){
  if (err && f){

    //    char s[1024];  strerror_r(err,s,1023);   fprintf(f," strerror_r=%s \n",s);
    fprintf(f," Error %d %s: %s ",err,error_symbol(err),strerror(err));
  }
}

//////////////
/// String ///
//////////////
static int cg_empty_dot_dotdot(const char *s){
  return !s || !*s || (*s=='.' && (!s[1] || (s[1]=='.' && !s[2])));
}
static char *cg_strncpy(char *dst,const char *src, int n){
  *dst=0;
  if (src) strncat(dst,src,n);
  return dst;
}
#define SNPRINTF(dest,max,...)   (max<=snprintf(dest,max,__VA_ARGS__) && (log_error("Exceed snprintf "),true))
static uint32_t cg_strlen(const char *s){
  return s?strlen(s):0;
}
static const char* snull(const char *s){ return s?s:"Null";}
static MAYBE_INLINE char *yes_no(int i){ return i?"Yes":"No";}
#define LASTCHAR(x) x[sizeof(x)-2]
#define STRLEN(ending) ((int)sizeof(ending)-1)
#define ENDSWITH(s,slen,ending)  ((slen>=STRLEN(ending)) && s[slen-1]==LASTCHAR(ending) && (!memcmp(s+slen-STRLEN(ending),ending,STRLEN(ending))))
#define ENDSWITHI(s,slen,ending) ((slen>=STRLEN(ending)) && (s[slen-1]|32)==(32|LASTCHAR(ending)) && (!strcasecmp(s+slen-STRLEN(ending),ending)))

static bool cg_endsWith(const char* s,int s_l,const char* e,int e_l){
  if (!s || !e) return false;
  if (!s_l) s_l=strlen(s);
  if (!e_l) e_l=strlen(e);
  return e_l<=s_l && 0==memcmp(s+s_l-e_l,e,e_l);
}

static bool cg_startsWith(const char* s,int s_l,const char* e,int e_l){
  if (!s || !e) return false;
  if (!s_l) s_l=strlen(s);
  if (!e_l) e_l=strlen(e);
  return e_l<=s_l && 0==memcmp(s,e,e_l);
}

static bool cg_endsWithZip(const char *s, int len){
  if(!len)len=cg_strlen(s);
  return s && ENDSWITHI(s,len,".zip");
}
static bool cg_endsWithDotD(const char *s, int len){
  if(!len)len=cg_strlen(s);
  return s && ENDSWITHI(s,len,".d");
}


/*
  static bool equalsSlash(const char *s){
  return s && *s=='/' && !s[1];
  }
  static int cg_count_slash(const char *p){
  const int n=cg_strlen(p);
  int count=0;
  RLOOP(i,n) if (p[i]=='/') count++;
  return count;
  }
*/
static int cg_last_slash(const char *path){
  RLOOP(i,cg_strlen(path)){
    if (path[i]=='/') return i;
  }
  return -1;
}

#define OPT_STR_REPLACE_DRYRUN (1<<0)
#define OPT_STR_REPLACE_ASSERT (1<<1)
static int cg_str_replace(int opt,char *haystack,  int h_l, const char *needle,  int n_l, const char *replacement,  int r_l){
  assert(haystack!=NULL);assert(needle!=NULL);assert(replacement!=NULL);
  if (!h_l) h_l=strlen(haystack);
  if (!r_l) r_l=strlen(replacement);
  if (!n_l) n_l=strlen(needle);
  const bool ends_null=haystack[h_l]==0;
  //fprintf(stderr,"lllllllllllllllllll h_l=%d  n_l=%d   r_l=%d  \n",h_l,n_l,r_l);
  assert(n_l>0);
  bool replaced=false;
  RLOOP(h,h_l-n_l+1){
    if (haystack[h]!=needle[0] || memcmp(haystack+h,needle,n_l)) continue;
    replaced=true;
    const int diff=r_l-n_l;
    h_l+=diff;
    if (0==(opt&OPT_STR_REPLACE_DRYRUN)){
      if (diff<0){ /* Shift left, gets-smaller */
        FOR(p,h+r_l,h_l) haystack[p]=haystack[p-diff];
      }else if (diff>0){ /* Shift right */
        for(int p=h_l; --p>=h+r_l;) haystack[p]=haystack[p-diff];
      }
      memcpy(haystack+h,replacement,r_l);
    }
  }
  if (0!=(opt&OPT_STR_REPLACE_ASSERT)){
    //fprintf(stderr,"needle=%s replacement=%s",needle,replacement);
    //fprintf(stderr,ANSI_FG_BLUE"%s"ANSI_RESET,haystack);
    assert(replaced);
  }
  if (0==(opt&OPT_STR_REPLACE_DRYRUN) && ends_null) haystack[h_l]=0;
  return h_l;
}




#define OPT_CG_STRSPLIT_WITH_EMPTY_TOKENS (1<<8)
static int cg_strsplit(const int opt_and_sep, char *s, const int s_l, char *tokens[], int *tokens_l){
  bool prev_sep=true;
  int count=0;
  char *tok=NULL;
  if (s){
    for(int i=0;;i++){
      const bool isend=s_l?(i>=s_l):!s[i];
      const bool issep=isend || s[i]==(opt_and_sep&0xff);
      if (prev_sep &&  ( (opt_and_sep&OPT_CG_STRSPLIT_WITH_EMPTY_TOKENS) || !issep)){
        tok=s+i;
        if (tokens) tokens[count]=tok;
      }
      if (tok && issep){
        if (tokens_l) tokens_l[count]=s+i-tokens[count];
        count++;
        tok=NULL;
      }
      if (isend) break;
      prev_sep=issep;
    }
  }
  if (tokens) tokens[count]=NULL;
  return count;
}


///////////////////
/// time       ///
///////////////////


static struct timeval  _startTime;
static int64_t currentTimeMillis(){
  struct timeval tv={0};
  gettimeofday(&tv,NULL);
  return tv.tv_sec*1000+tv.tv_usec/1000;
}
static int deciSecondsSinceStart(){
  if (!_startTime.tv_sec) gettimeofday(&_startTime,NULL);
  struct timeval now;
  gettimeofday(&now,NULL);
  return (int)((now.tv_sec-_startTime.tv_sec)*10+(now.tv_usec-_startTime.tv_usec)/100000);
}

///////////////////
/// file path   ///
///////////////////



// static int slash_not_trailing(const char *path){ const char *p=strchr(path,'/');  return p && p[1]?(int)(p-path):-1; }
static int cg_pathlen_ignore_trailing_slash(const char *p){
  const int n=cg_strlen(p);
  return n && p[n-1]=='/'?n-1:n;
}
static bool cg_path_equals_or_is_parent(const char *subpath,const int subpath_l,const char *path,const int path_l){
  return subpath && path && (subpath_l==path_l||(subpath_l<path_l&&path[subpath_l]=='/')) && !memcmp(path,subpath,subpath_l);
}

enum validchars{VALIDCHARS_PATH,VALIDCHARS_FILE,VALIDCHARS_NOQUOTE,VALIDCHARS_NUM};
static bool *cg_validchars(enum validchars type){
  static bool ccc[VALIDCHARS_NUM][128];
  static bool initialized;
  if (!initialized){
    if (type==VALIDCHARS_FILE||type==VALIDCHARS_PATH||type==VALIDCHARS_NOQUOTE){
      for(int t=VALIDCHARS_NUM;--t>=0;){
        bool *cc=ccc[t];
        FOR(i,'A','Z'+1) cc[i|32]=cc[i]=true;
        FOR(i,'0','9'+1) cc[i]=true;
        cc['=']=cc['+']=cc['-']=cc['_']=cc['$']=cc['@']=cc['.']=cc['~']=true;
      }
    }
    ccc[VALIDCHARS_PATH]['/']=ccc[VALIDCHARS_PATH][' ']=ccc[VALIDCHARS_FILE][' ']=ccc[VALIDCHARS_NOQUOTE]['/']=true;
    ccc[VALIDCHARS_NOQUOTE][':']=true;

    initialized=true;
  }
  return ccc[type];
}

static int cg_find_invalidchar(enum validchars type,const char *s,const int len){
  if (s){
    const bool *bb=cg_validchars(type);
    FOR(i,0,len){

      if (s[i]<0||s[i]>127||!bb[s[i]]) return i;
    }
  }
  return -1;
}




static int cg_path_for_fd(const char *title, char *path, int fd){
  *path=0;
  if (!has_proc_fs()) return 1;
  char buf[99];
  sprintf(buf,"/proc/self/fd/%d",fd);

  const ssize_t n=readlink(buf,path, MAX_PATHLEN-1);
  if (n<0){
    log_errno("\n%s  %s: ",snull(title),buf);
    return -1;
  }
  return path[n]=0;
}

static int cg_count_fd_this_prg(){
  int n=0;
  if (has_proc_fs()){
    DIR *dir=opendir("/proc/self/fd");
    while(readdir(dir)) n++;
    closedir(dir);
  }
  return n;
}

static bool cg_check_path_for_fd(const char *title, const char *path, int fd){
  char check_path[MAX_PATHLEN],rp[PATH_MAX];
  if (!realpath(path,rp)){
    log_error("%s  Failed realpath(%s)\n",snull(title),path);
    return false;
  }
  cg_path_for_fd(title,check_path,fd);
  if (strncmp(rp,path,MAX_PATHLEN-1)){
    log_error("%s  fd=%d,%s   D_VP(d)=%s   realpath(path)=%s\n",title,fd,check_path,path,rp);
    return false;
  }
  return true;
}

static void cg_print_path_for_fd(int fd){
  char buf[99],path[512];
  if (!has_proc_fs()){
    fprintf(stderr,ERROR_MSG_NO_PROC_FS"\n");
  }else{
    sprintf(buf,"/proc/self/fd/%d",fd);
    const ssize_t n=readlink(buf,path,511);
    if (n<0){
      log_errno("%s  No path",buf);
    }else{
      path[n]=0;
      fprintf(stderr,"Path for %d:  %s\n",fd,path);
    }
  }
}
///////////////////
/// Arithmetics ///
///////////////////

static int isPowerOfTwo(unsigned int n){
  return n && (n&(n-1))==0;
}

/* Integer sqrt from https://en.wikipedia.org/wiki/Integer_square_root */
static unsigned int isqrt(unsigned int y){
  unsigned int L=0,M,R=y+1;
  while(L!=R-1){
    M=(L+R)/2;
    if (M*M<=y) L=M; else R=M;
  }
  return L;
}
static bool is_square_number(unsigned int y){
  unsigned int s=isqrt(y);
  return s*s==y;
}

/* static inline int MAX_int(int a,int b){ return MAX(a,b);} */
/* static inline int64_t MIN_long(int64_t a,int64_t b){ return MIN(a,b);} */
/* static inline int64_t MAX_long(int64_t a,int64_t b){ return MAX(a,b);} */
static MAYBE_INLINE int64_t cg_atol_kmgt(const char *s){
  if (!s) return 0;
  char *c=(char*)s;
  while(*c && '0'<=*c && *c<='9') c++;
  *c&=~32;
  return atol(s)<<(*c=='K'?10:*c=='M'?20:*c=='G'?30:*c=='T'?40:0);
}
static void cg_log_file_mode(mode_t m){
  char mode[11];
  int i=0;
  mode[i++]=S_ISDIR(m)?'d':'-';
#define C(m,f) mode[i++]=(m&S_IRUSR)?f:'-';
  C(S_IRUSR,'r');C(S_IWUSR,'w');C(S_IXUSR,'x');
  C(S_IRGRP,'r');C(S_IWGRP,'w');C(S_IXGRP,'x');
  C(S_IROTH,'r');C(S_IWOTH,'w');C(S_IXOTH,'x');
#undef C
  mode[i++]=0;
  fputs(mode,stderr);
}


//
///////////////////
/// file stat   ///
///////////////////

#define cg_log_file_stat(...) _cg_log_file_stat(__func__,__VA_ARGS__)
static void _cg_log_file_stat(const char *fn,const char * name,const struct stat *s){
  char *color=ANSI_FG_BLUE;
#ifdef SHIFT_INODE_ROOT
  if (s->st_ino>(1L<<SHIFT_INODE_ROOT)) color=ANSI_FG_MAGENTA;
#endif
  fprintf(stderr,"%s() %s  size=%lld blksize=%lld blocks=%lld links=%u inode=%s%llu"ANSI_RESET" dir=%s uid=%u gid=%u ",fn,name,(LLD)s->st_size,(LLD)s->st_blksize,(LLD)s->st_blocks,  (uint32_t) s->st_nlink,color,(LLU)s->st_ino,  yes_no(S_ISDIR(s->st_mode)), s->st_uid,s->st_gid);
  //st_blksize st_blocks f_bsize
  cg_log_file_mode(s->st_mode);
  fputc('\n',stderr);
}
static void cg_log_open_flags(int flags){
  fprintf(stderr,"flags=%x{",flags);
#define C(a) if (flags&a) fprintf(stderr,#a" ")
  C(O_RDONLY); C(O_WRONLY);C(O_RDWR);C(O_APPEND);C(O_CLOEXEC);C(O_CREAT);C(O_DIRECTORY);C(O_DSYNC);C(O_EXCL);C(O_NOCTTY);C(O_NOFOLLOW);C(O_NONBLOCK);C(O_SYNC);C(O_TRUNC);
#ifdef O_DIRECT
  C(O_DIRECT);
#endif
#ifdef O_PATH
  C(O_PATH);
#endif
#ifdef O_LARGEFILE
  C(O_LARGEFILE);
#endif

#ifdef O_NOATIME
  C(O_NOATIME);
#endif
#ifdef O_TMPFILE
  C(O_TMPFILE);
#endif
#ifdef O_ASYNC
  C(O_ASYNC);
#endif
#ifdef O_NDELAY
  C(O_NDELAY);
#endif

#undef C
  fputc('}',stderr);
}


static void cg_clear_stat(struct stat *st){ if(st) memset(st,0,sizeof(struct stat));}
static bool cg_stat_differ(const char *title,struct stat *s1,struct stat *s2){
  if (!s1||!s2) return false; // memcmp would lead to false positives
  char *wrong=NULL;
#define C(f) (wrong=#f,s1->f!=s2->f)
  if (C(st_ino)||C(st_mode)||C(st_uid)||C(st_gid)||C(st_size)||C(st_blksize)||C(st_blocks)||C(st_mtime)||C(st_ctime)||(wrong=NULL,false)){
    log_warn("stat_t.%s\n",wrong);
    cg_log_file_stat(title,s1);
    cg_log_file_stat(title,s2);
    return true;
  }
#undef C
  //  log_succes("Stat are identical: %s\n",title);
  return false;
}
#define cg_is_dir(f) cg_is_stat_mode(S_IFDIR,f)
#define cg_is_symlink(f) cg_is_stat_mode(S_IFLNK,f)
#define cg_is_regular_file(f) cg_is_stat_mode(S_IFREG,f)
static bool cg_is_stat_mode(const mode_t mode,const char *f){
  struct stat st={0};
  return !lstat(f,&st) &&  (st.st_mode&S_IFMT)==mode;
}

static bool cg_access_from_stat(const struct stat *stats,int mode){ // equivaletn to access(path,mode)
  int granted;
  mode&=(X_OK|W_OK|R_OK);
#if R_OK!=S_IROTH || W_OK!=S_IWOTH || X_OK!=S_IXOTH
  ?error Oops, portability assumptions incorrect.;
#endif
  if (mode==F_OK) return 0;
  if (getuid()==stats->st_uid)
    granted=(unsigned int) (stats->st_mode&(mode<<6))>>6;
  else if (getgid()==stats->st_gid || cg_group_member(stats->st_gid))
    granted=(unsigned int) (stats->st_mode&(mode<<3))>>3;
  else
    granted=(stats->st_mode&mode);
  return granted==mode;
}
static bool cg_file_set_atime(const char *path, struct stat *stbuf,long secondsFuture){
  struct stat st;
  if (!stbuf && stat(path,stbuf=&st)) return false;
  log_debug_now("secondsFuture=%ld\n",secondsFuture);
  struct utimbuf new_times={.actime=time(NULL)+secondsFuture,.modtime=stbuf->st_mtime};
  return !utime(path,&new_times);
}
///////////////////
/// file        ///
///////////////////
/* write() may be write only part of the data */
static bool cg_fd_write(int fd,char *t,const off_t size0){
  for(off_t size=size0; size>0;){
    off_t n=write(fd,t,size);
    if (n<0) return false;
    t+=n;
    size-=n;
  }
  return true;
}

static bool cg_fd_write_str(int fd,char *t){
  return t && cg_fd_write(fd,t,strlen(t));
}


static int cg_symlink_overwrite_atomically(const char *src,const char *lnk){
  if (!cg_is_symlink(lnk)) unlink(lnk);
  char lnk_tmp[MAX_PATHLEN];
  strcpy(lnk_tmp,lnk);strcat(lnk_tmp,".tmp");
  unlink(lnk_tmp);
  symlink(src,lnk_tmp);
  return rename(lnk_tmp,lnk);
}
static void cg_print_substring(int fd,const char *s,int f,int t){  write(fd,s,MIN_int(cg_strlen(s),t)); }


static bool cg_mkdir(const char *path,const mode_t mode){
  return path && (!mkdir(path,mode) || errno==EEXIST);
}
static bool _cg_recursive_mkdir(const bool parentOnly,const char *path){
  if (!path) return false;
  char p[PATH_MAX];
  strcpy(p,path);
  const int n=cg_pathlen_ignore_trailing_slash(p);

  FOR(i,2,n){
    if (p[i]=='/'){
      p[i]=0;
      if (!cg_mkdir(p,S_IRWXU)) return false;
      p[i]='/';
    }
  }
  if (!parentOnly &&  !cg_mkdir(p,S_IRWXU)) return false;
  return true;
}
#define cg_recursive_mkdir(path) _cg_recursive_mkdir(false,path)
#define cg_recursive_mk_parentdir(path) _cg_recursive_mkdir(true,path)


static void log_list_filedescriptors(const int fd){
  if (!has_proc_fs()){
    fprintf(stderr,ERROR_MSG_NO_PROC_FS"n");
  }else{

    const pid_t pid0=getpid();
    if (!fork()){
      char path[33];
      sprintf(path,"/proc/%d/fd",pid0);
      execl("/usr/bin/ls","/usr/bin/ls",path,NULL);
    }
  }
}

static char *cg_copy_path(char *dst,const char *src){
  if (*src=='~'){
    assert(src[1]=='/');
    sprintf(dst,"%s%s",getenv("HOME"),src+1);
  }else{
    strcpy(dst,src);
  }
  return dst;
}

///////////////////
///    time     ///
///////////////////
static double cg_timespec_diff(const struct timespec a, const struct timespec b) {
  double v= (a.tv_sec-b.tv_sec)+(a.tv_nsec-b.tv_nsec)/(1000*1000*1000.0);
  return v;
}

static double cg_timespec_diff_lt(const struct timespec a, const struct timespec b,const double threshold) {
  double dsec=a.tv_sec-b.tv_sec;
  return dsec+1<=threshold ||  (threshold-dsec)*(1000*1000*1000.0)<(a.tv_nsec-b.tv_nsec);
}

static bool cg_timespec_b_before_a(struct timespec a, struct timespec b) {  //Returns true if b happened first.
  if (a.tv_sec==b.tv_sec) return a.tv_nsec>b.tv_nsec;
  return a.tv_sec>b.tv_sec;
}




#define CG_TIMESPEC_EQ(a,b) (a.tv_sec==b.tv_sec && a.tv_nsec==b.tv_nsec)
////////////////////
/// heap, memory ///
////////////////////

#define CG_REALLOC(type,pointer,expr) {type tmp=realloc(pointer,expr); if (!tmp){fprintf(stderr,"realloc failed.\n"); EXIT(1);};pointer=tmp;}
/////////////
////  id  ///
/////////////
static bool cg_is_member_of_group(char *group){
  int size=getgroups(0,NULL);
  gid_t gg[size];
  getgroups(size,gg);
  FOR(i,-1,size){
    struct group *g=getgrgid(i<0?getegid():gg[i]);
    if (!strcmp(group,g->gr_name)) return true;
  }

  return false;
}
#define HINT_GRP_DOCKER "The current user is not member of group 'docker'. See https://en.wikipedia.org/wiki/Docker_(software).  Docker based auto-generation wont work.\nConsider to run 'newgrp docker' before starting ZIPsFS.\n"
static bool cg_is_member_of_group_docker(){
  static int r=0;
  if (!r) r=cg_is_member_of_group("docker")?1:-1;
  return r==1;
}

///////////////////
///  process    ///
///////////////////
static bool cg_log_exec_fd(const int fd,  char * const env[],  char * const cmd[]){
  RLOOP(j,2){
    char **s=(char**)(j?env:cmd);
    if (s){
      cg_fd_write_str(fd,j?"ENVIRONMENT VARIABLES:\n":"COMMAND-LINE:\n");
      while(*s){
        cg_fd_write_str(fd,"  ");
        const char quote=cg_find_invalidchar(VALIDCHARS_NOQUOTE,*s,strlen(*s))<0?0 :strchr(*s,'\'')||strchr(*cmd,'\\')?'"':'\'';
        if (quote) write(fd,&quote,1);
        cg_fd_write_str(fd,*s++);
        if (quote) write(fd,&quote,1);
        if (j) cg_fd_write_str(fd,"\n");
      }
      cg_fd_write_str(fd,"\n");
      if (!j && *cmd && !strcmp(*cmd,"docker") && !cg_is_member_of_group_docker()) cg_fd_write_str(fd,HINT_GRP_DOCKER);
    }

  }
  cg_fd_write_str(fd,"\n");
  return true;
}
static bool cg_log_waitpid_status(FILE *f,const unsigned int status,const char *msg){
  int logged=0;
  if (status){
    FOR(j,0,f?2:1){
      if (logged && f) fputs("STATUS OF fork() - exec() - waitpid()\n",f);
#define C(m) if (m(status)){logged++; if (j) fprintf(f,"    %s",#m);}
      const int current=logged;
      C(WIFEXITED); C(WIFSIGNALED); C(WIFSTOPPED);   C(WIFCONTINUED);
      if (WIFSIGNALED(status)) C(WCOREDUMP);
      if (j && current!=logged) fputc('\n',f);
#undef C
#define C(m) if (m(status)){logged++; if(j) fprintf(f,"  %s=%u\n",#m,m(status));}
      if (WIFSIGNALED(status)) C(WTERMSIG);
      if (WIFSTOPPED(status))  C(WSTOPSIG);
      if (WIFEXITED(status)){
        C(WEXITSTATUS);
        if (j){fprint_strerror(f,WEXITSTATUS(status)); fputc('\n',f);}
      }
#undef C
      if (!logged) break;
    }
  }
  return logged;
}
static int cg_waitpid_logtofile_return_exitcode(int pid,const char *err){
  log_entered_function("err=%s\n",err);
  int status=-1;
  FILE *f=NULL;
  const int ret=waitpid(pid,&status,0);
  if (ret==-1){
    if (!f) f=fopen(err,"a");
    if (f){
      fputs("waitpid() failed.\n",f);
      fprint_strerror(f,errno);
      if (f) fclose(f);
    }
    return -1;
  }
  if (err && cg_log_waitpid_status(f,status,__func__) && !f) cg_log_waitpid_status(f=fopen(err,"a"),status,__func__);
  if (f) fclose(f);
  return WIFEXITED(status)?WEXITSTATUS(status):INT_MIN;
}

// #pragma GCC diagnostic ignored "-Wunused-variable" //__attribute__((unused));

static void cg_exec(char * const env[],char *cmd[],const int fd_out,const int fd_err){
  if(fd_out>0) dup2(fd_out,STDOUT_FILENO);
  if(fd_err>0) dup2(fd_err,STDERR_FILENO);
  if(fd_out>0) close(fd_out);
  if(fd_err>0 && fd_err!=fd_out) close(fd_err);
  cg_log_exec_fd(STDERR_FILENO,env,cmd);
  #if HAS_UNDERSCORE_ENVIRON
  extern char **_environ;
  if (env) _environ=(char**)env;
  #endif
  execvp(cmd[0],cmd);
  EXIT(EPIPE);
}


//////////////////////////////////////////////////////////////////////
// Compare text and file content
//////////////////////////////////////////////////////////////////////
#if CODE_NOT_NEEDED
#define DIFFERS_F_S_REPORT_NEXIST (1<<1)
static int differs_filecontent_from_string(const int opt,const char* path, const long seek,const char* text,const long text_l){
  const int fd=open(path,O_RDONLY);
  if (fd<0){ if (0!=(opt&DIFFERS_F_S_REPORT_NEXIST)) log_errno("open(\"%s\",O_RDONLY)",path); return -1;}
  char buf[4096];
  int n;
  long pos=0;
  while((n=read(fd,buf,sizeof(buf)))>0){
    RLOOP(i_buf,n){
      const long i_txt=pos-seek+i_buf;/* TODO inefficient */
      if (i_txt>=0 && i_txt<text_l){
        //        printf("%c %c \n", text[i_txt],buf[i_buf]);
        if (text[i_txt]!=buf[i_buf]) return true;
      }
    }
    pos+=n;
  }
  if (pos<seek+text_l) return true;
  if (n<0) log_errno("read(fd,...) %s",path);
  close(fd);
  return 0;
}
#endif //CODE_NOT_NEEDED



#endif // _cg_utils_dot_c
// 1111111111111111111111111111111111111111111111111111111111111
#if defined __INCLUDE_LEVEL__ && __INCLUDE_LEVEL__==0

  {  char m[10]; memset(m,9,10000); return 0;}
  switch(8){
  case 0:{
    bool *ccpath=cg_validchars(VALIDCHARS_PATH);
    fprintf(stderr,"ccpath\n");
    FOR(c,0,256){
      if (ccpath[c]) putc(c,stderr);
    }
    fprintf(stderr,"\n");
    fprintf(stderr,"%s  %d\n",__func__,cg_find_invalidchar(VALIDCHARS_PATH,argv[1],strlen(argv[1])));
  } break;
  case 1:{
    struct stat stbuf;
    const char *path=argv[1];
    stat(path,&stbuf);
    cg_file_set_atime(path,&stbuf,3600L*atoi(argv[2]));
  } break;
  case 2:{
    char *h=malloc(9999);
    strcpy(h,argv[1]);
    int l1=cg_str_replace(OPT_STR_REPLACE_DRYRUN,h,0, argv[2],0,argv[3],0);

    int l2=cg_str_replace(0,h,0, argv[2],0,argv[3],0);
    printf("l1=%d l2=%d h=%s\n",l1,l2,h);
    free(h);
  } break;
  case 3:{
  } break;
  case 4:{
    int tokens_l[99];
    char *tokens[99];
    const int n=cg_strsplit(':',argv[1],0,NULL,NULL);
    cg_strsplit(':',argv[1],0,tokens,tokens_l);
    printf("n=%d\n",n);
    char token[999];
    FOR(i,0,n){
      strncpy(token,tokens[i],tokens_l[i]);
      token[tokens_l[i]]=0;
      printf("%d/%d %s  %d\n",i,n,token,tokens_l[i]);
    }
  }
  case 5:{
    char *s=strdup("hello");
    CG_REALLOC(char *,s,10);
  } break;
  case 6:{
    int a=atoi(argv[1]),b=atoi(argv[2]);
    int c=(2);
    printf("max(%d,%d)=%d\n",a,b,c);
  } break;
  case 7:{
    //    log_verbose("cg_find_invalidchar=%d\n",cg_find_invalidchar(VALIDCHARS_PATH,argv[1],strlen(argv[1])));    EXIT(1);
    char *env[]={"a=1",NULL};
    char *cmd[]={"ls","-l","space char","backslash\\","single'quote'",NULL};
    cg_log_exec_fd(2,env,cmd);
  } break;
  case 8:{
    printf("isqrt=%d\n",isqrt(atoi(argv[1])));
  } break;

  }
}

#endif
