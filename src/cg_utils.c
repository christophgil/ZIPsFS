#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MAX_PATHLEN 512
#define DEBUG_NOW 1
#define FREE(s) free((void*)s),s=NULL
/*********************************************************************************/
/* *** String *** */
int empty_dot_dotdot(const char *s){  return !*s || (*s=='.' && (!s[1] || (s[1]=='.' && !s[2]))); }
char *my_strncpy(char *dst,const char *src, int n){
  *dst=0;
  if (src) strncat(dst,src,n);
  return dst;
}

#define SNPRINTF(dest,max,...)   (max<=snprintf(dest,max,__VA_ARGS__) && (log_error("Exceed snprintf "),true))
unsigned int my_strlen(const char *s){ return !s?0:strnlen(s,MAX_PATHLEN);}
const char* snull(const char *s){ return s?s:"Null";}
static inline char *yes_no(int i){ return i?"Yes":"No";}
static bool endsWith(const char* s,const char* e){
  if (!s || !e) return false;
  const int sn=strlen(s),en=strlen(e);
  return en<=sn && 0==strcmp(s+sn-en,e);
}
int count_slash(const char *p){
  const int n=my_strlen(p);
  int count=0;
  for(int i=n;--i>=0;) if (p[i]=='/') count++;
  return count;
}
int last_slash(const char *path){
  for(int i=my_strlen(path);--i>=0;){
    if (path[i]=='/') return i;
  }
  return -1;
}
static int slash_not_trailing(const char *path){
  char *p=strchr(path,'/');
  return p && p[1]?(int)(p-path):-1;
}
int pathlen_ignore_trailing_slash(const char *p){
  const int n=my_strlen(p);
  return n && p[n-1]=='/'?n-1:n;
}
bool equivalent_path(char *nextPath, const char *path,int equiv){
  *nextPath=0;
#define SLASH_EQUIVALENT "/EquiValent/"
  const int EQUIVALENT_L=sizeof(SLASH_EQUIVALENT)-1;
  const char* e=equiv?strstr(path,SLASH_EQUIVALENT):0,*slash=e?strchr(e+EQUIVALENT_L,'/'):NULL;
  if (slash){
    memcpy(nextPath,path,e-path+EQUIVALENT_L);
    sprintf(nextPath+(e-path+EQUIVALENT_L),"%d%s",atoi(e+EQUIVALENT_L)+equiv,slash);
  }else{
    strcpy(nextPath,path);
  }
  return slash!=NULL;
#undef SLASH_EQUIVALENT
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
static int min_int(int a,int b){ return MIN(a,b);}
static int max_int(int a,int b){ return MAX(a,b);}
/*********************************************************************************/
/* *** time *** */
long currentTimeMillis(){
  struct timeval tv={0};
  gettimeofday(&tv,NULL);
  return tv.tv_sec*1000+tv.tv_usec/1000;
}

/*********************************************************************************/
/* *** stat *** */
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/statfs.h>
void clear_stat(struct stat *st){ if(st) memset(st,0,sizeof(struct stat));}
static long time_ms(){
  struct timeval tp;
  gettimeofday(&tp,NULL);
  return tp.tv_sec*1000+tp.tv_usec/1000;
}
#define SHIFT_INODE_ROOT 40
#define SHIFT_INODE_ZIPENTRY 43
#define ADD_INODE_ROOT(root) (((long)root+1)<<SHIFT_INODE_ROOT)
void log_file_stat(const char * name,const struct stat *s){
  char *color= (s->st_ino>(1L<<SHIFT_INODE_ROOT)) ? ANSI_FG_MAGENTA:ANSI_FG_BLUE;
  printf("%s  size=%lu blksize=%lu blocks=%lu links=%lu inode=%s%lu"ANSI_RESET" dir=%s uid=%u gid=%u ",name,s->st_size,s->st_blksize,s->st_blocks,   s->st_nlink,color,s->st_ino,  yes_no(S_ISDIR(s->st_mode)), s->st_uid,s->st_gid);
  //st_blksize st_blocks f_bsize
  putchar(  S_ISDIR(s->st_mode)?'d':'-');
  putchar( (s->st_mode&S_IRUSR)?'r':'-');
  putchar( (s->st_mode&S_IWUSR)?'w':'-');
  putchar( (s->st_mode&S_IXUSR)?'x':'-');
  putchar( (s->st_mode&S_IRGRP)?'r':'-');
  putchar( (s->st_mode&S_IWGRP)?'w':'-');
  putchar( (s->st_mode&S_IXGRP)?'x':'-');
  putchar( (s->st_mode&S_IROTH)?'r':'-');
  putchar( (s->st_mode&S_IWOTH)?'w':'-');
  putchar( (s->st_mode&S_IXOTH)?'x':'-');
  putchar('\n');
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

int is_regular_file(const char *path){
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
void print_substring(int fd,char *s,int f,int t){  write(fd,s,min_int(my_strlen(s),t)); }
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
