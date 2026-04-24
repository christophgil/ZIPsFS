//////////////////////////////////////////////////////////////////
/// Logging in  ZIPsFS                                         ///
/// This file provides                                         ///
///   cg_copy_url_or_file() Needed by ZIPsFS_preloaddisk.c ///
///   cg_download_url() Needed by ZIPsFS_internet.c            ///
//////////////////////////////////////////////////////////////////

// cppcheck-suppress-file unusedFunction
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#include "cg_stacktrace.c"
#include <zlib.h>
#endif


#ifndef _cg_download_file_dot_c
#include "cg_utils.c"
#include "cg_exec_pipe.c"

#define E(msg) perror(__FILE__":"STRINGIZE(__LINE__)" "msg);

enum {COPY_HEADER=1<<14,COPY_USE_DOT_URL_FILE=1<<15,COPY_ADD_STDERR=1<<16};
#define _COPY_FLAGS_BEGIN (COPY_HEADER)
_Static_assert(!(COMPRESSION_MASK&_COPY_FLAGS_BEGIN),"COMPRESSION_MASK");

enum {_CG_DOWNLOAD_URL_CMD=16};
static long cg_read_fd(const int in, const int out,void (*progress)(void*), void *progress_para){
  errno=0;
  char buf[16*1024];
  int n;
  long count=0;
  for (int i=0; (n=read(in,buf,sizeof(buf)-1))>0; i++){
    if (progress) progress(progress_para);
    cg_fd_write(out,buf,n);
    count+=n;
    if (WITH_DEBUG_THROTTLE_DOWNLOAD){
      //if (!(i%16))
      fputc('l',stderr);
      usleep(10*1000);
    }
  }
  close(in);
  return n<0?-1:count;
}




#define OPT_AND_COMPRESS()  const int opt=opt_and_compress&~COMPRESSION_MASK, iCompress=opt_and_compress&COMPRESSION_MASK

/*************************/
/* Downloading / Copying */
/* Entry point for ZIPsFS_internet.c */
/*************************/
static bool cg_cmd_curl(const int opt,const char *cmd[],const char *url){
  if (!is_installed_curl()){ log_error("curl is not installed.");  return false;}
  int k=0;
#define P(a) cmd[k++]=a;
  P("curl")P("-o")P("-");
  if (opt&COPY_HEADER) P("-I");
#ifdef CURL_OPTS
  const char *extra[]={CURL_OPTS NULL};
  for(const char **p=extra; *p; p++) P(*p);
#endif //CURL_OPTS
  P(url)P(NULL)assert(k<_CG_DOWNLOAD_URL_CMD);
#undef P
  return true;
}

// cppcheck-suppress nullPointerRedundantCheck
static bool cg_download_url(const int opt_and_compress, const char *url, const char *outfile, void (*progress)(void*), void *progress_para){
  OPT_AND_COMPRESS(); // rph
  log_entered_function("header:%d  url:%s outfile:'%s' iCompress=%d",(opt_and_compress&COPY_HEADER),url,outfile,iCompress); // cppcheck-suppress [ctunullpointer,nullPointerRedundantCheck]
  const char *cmd[_CG_DOWNLOAD_URL_CMD]; if (!cg_cmd_curl(opt,cmd,url)) return false;
  TMP_FOR_FILE(tmp,outfile);
  int fdout=0;
  if (outfile && (fdout=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,0644))==-1){ log_errno("open(%s)",outfile); return false;}
  bool ok=true;

  if (iCompress){
    const char *decomp[DECOMPRESS_CMD_MAX+1]={0};
    if (iCompress!=COMPRESSION_gz) cg_cmd_decompress(iCompress,decomp,NULL);
    log_debug_now("cmd0=%s",decomp[0]);
    ok=cg_exec_pipe(cmd,NULL,
                    *decomp?decomp:_PSEUDO_CMD_ZLIB,
                    // style: Condition '*decomp' is always false [knownConditionTrueFalse]
                    fdout,progress,progress_para);
    if (!ok) log_warn("cg_exec_pipe %s",cmd[0]);
  }else{
    ok=cg_fork_exec(cmd,NULL,0,fdout,0);
  }
  if (!outfile) return ok;
  close(fdout);
  if (!ok){
    log_errno("curl  decompress: '%s'",cg_compression_file_ext(iCompress,NULL));
    unlink(tmp);
    return false;
  }
  ok=cg_rename_tmp_outfile(tmp,outfile);
  return ok;
}








/*******************************************************************************/
/* Unless the local file dst exists, it will be downloaded. */
/* dst will contain the downloaded path.                                       */
/* If parameter zpath is NULL, it will be obtained from parameter d.           */
/*******************************************************************************/


/******************************************************/
/* Has a path component a corresponding dot-URL file? */
/******************************************************/
static int cg_read_file_into_buffer(char *buf, const int buf_l, const char *path){
  const int fd=open(path,O_RDONLY);
  if (fd<2) return -1;
  int pos=0,n;
  while (buf_l>pos && (n=read(fd,buf+pos,buf_l-pos))>0) pos+=n;
  close(fd);
  return pos;
}
static bool cg_url_by_dot_url_file(char url[PATH_MAX+1], const char *root_or_null, const char *vp){
  const int vp_l=strlen(vp),root_l=cg_strlen(root_or_null);
  if (ENDSWITH(vp,vp_l,".URL")) return false;
  char tmp[root_l+vp_l+6];
  const int tmp_l=stpcpy(stpcpy(tmp,root_or_null?root_or_null:""),vp)-tmp;
  tmp[tmp_l]=0;
  for(int i=tmp_l+1; --i>=root_l;){
    if (i!=tmp_l && tmp[i]!='/') continue;
    stpcpy(tmp+i,".URL");
    int url_l=cg_read_file_into_buffer(url,MAX_PATHLEN,tmp);
    while(url_l>0 && isspace(url[url_l-1])) url_l--;
    if (url_l>0){
      url[url_l]=0;
      if (url_l+vp_l-i+root_l>PATH_MAX){ log_error("URL exceeds "STRINGIZE(PATH_MAX)" characters: %s",tmp);continue;}
      stpcpy(url+url_l,vp+i-root_l);
      return true;
    }
  }
  return false;
}


static bool cg_url_associated_with_path(char url[PATH_MAX+1], const char *path){
  *url=0;
  if (cg_isURL(path)){
    strcpy(url,path);
    return true;
  }
  if (cg_url_by_dot_url_file(url,NULL,path)) return true;
  { // AVFS
    const char *found=strstr(path,"/#ftp:");
    if (found){
      strcpy(url,strchr(found,':')+1);
      return true;
    }
  }
  return false;
}



/*********************************************/
/* Entry point for ZIPsFS_preloaddisk.c  */
/*********************************************/

static bool cg_copy_url_or_file(const int iCompress,const char *src_path_without_ext, const char *dst, void (*progress)(void*), void *progress_para){
  char src_path[strlen(src_path_without_ext)+COMPRESSION_EXT_MAX_LEN];
  stpcpy(stpcpy(src_path,src_path_without_ext),cg_compression_file_ext(iCompress,NULL));
  char src[PATH_MAX+1];
  if (!realpath(src_path,src)){ log_errno("%s",src_path); return false;}
  {
    char url[PATH_MAX+1];
    cg_url_associated_with_path(url,src);
    if (*url) return cg_download_url(iCompress,url,dst,progress,progress_para);
  }
  errno=0;
  TMP_FOR_FILE(tmp,dst);
  const int out=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,0644);
  if (out<3){ log_errno("open(%s)",tmp); return false;}
  const char *cmd[DECOMPRESS_CMD_MAX+1];
  bool ok=cg_cmd_decompress(iCompress,cmd,src);
  log_debug_now("cmd0=%s ok=%i",cmd[0],ok);
  if (ok){
    ok=cg_fork_exec(cmd,NULL,0,out,0);
  }else{
    const int in=open(src,O_RDONLY);
    if (in<3){ log_errno("open(%s)",tmp); return false;}
    ok=(iCompress==COMPRESSION_gz?
        cg_read_gzip(in,out,progress,progress_para):
        cg_read_fd(  in,out,progress,progress_para))>0;
  }
  if (!ok) {unlink(tmp);return false;}
  return cg_rename_tmp_outfile(tmp,dst);
}


/************/
/* HTTP FTP */
/************/
static time_t cg_httpheader_parse_date(const char *s){
  struct tm g={0};
  char M[4];
  const int n=sscanf(s," %*[a-zA-Z,] %d %3s %d %d:%d:%d",&g.tm_mday,M, &g.tm_year, &g.tm_hour, &g.tm_min, &g.tm_sec);
  if (n!=6){ log_warn("Parsed only %d fields for '%s'",n,s); return 0;}
  const int hit=cg_str_str("JanFebMarAprMayJunJulAugSepOctNovDec",M);
  if (hit<0){ log_warn("Cannot parse month '%s'",s); return 0;}
  g.tm_mon=hit/3;
  g.tm_year-=1900;
  return timegm(&g);
}


static void cg_httpheader_parse_line(cg_httpheader_t *h,const char *line){
  while(!*line && isspace(*line)) line++;
  if (!*line) return;
  h->lines++;
  /* Header field names are case-insensitive. */

#define I(P) if (!strncasecmp(line,P,sizeof(P)-1))
  // cppcheck-suppress-macro unreadVariable
#define F(P,code) I(P) {h->is_header=true;const int l=sizeof(P)-1;{code;}}
  I("HTTP"){
    const char *spc=strchr(line,' ');
    if (spc){
      const int code=atoi(spc+1);
      if (code==404) h->is_404=true;
    }
  }
  F("Last-Modified:",h->mtime=cg_httpheader_parse_date(line+l));
  F("Content-Length:",h->size=atol(line+l));
  //if (iCompress) h->size=closest_with_identical_digits(10*st->st_size); /* Guess file size and make it a Schnapszahl */
  F("Content-type:",);
  I("Content-Location:") h->has_content_location=true; /* http://hgdownload.soe.ucsc.edu/goldenPath/archive/susScr3/uniprot/2022_05/UP000005640_9606.fasta.gz without gz */
#undef F
#undef I
}

static int cg_httpheader_read_fd(cg_httpheader_t *h,const int fd){
  FILE *f=fdopen(fd,"r");
  if (!f){
    log_errno("fdopen(%d)",fd);
    close(fd);
    return errno?errno:-1;
  }
  char *line=NULL;
  size_t line_l=0;
  for(ssize_t n; (n=getline(&line,&line_l,f))!=-1; ){
    cg_httpheader_parse_line(h,line);
  }
  fclose(f);
  return 0;
}


#define E(msg) perror(__FILE__":"STRINGIZE(__LINE__)" "msg);


static int _pipe_curl(const int opt_curl,const char *url, int pipefd[2], int *pid){
  const char *cmd[_CG_DOWNLOAD_URL_CMD]; if (!cg_cmd_curl(opt_curl,cmd,url)) return false;
  if (pipe(pipefd)==-1){ E("pipe");return false;}
*pid=fork();
  if (*pid<0){ E("fork()"); return false;}
  if (!*pid){
    close(pipefd[0]);
    int ev=cg_exec(cmd,NULL,0,pipefd[1],0);
    exit(ev);
  }
  close(pipefd[1]);
  return pipefd[0];
}
#undef E
#define CG_WAITPID(pid,src) _viamacro_cg_waitpid(pid,src,__func__,__LINE__)
static bool _viamacro_cg_waitpid(const int pid,const char *src, const char *func, const int line){
  int wstatus=0;
  waitpid(pid,&wstatus,0);
  const int e=WIFEXITED(wstatus)?WEXITSTATUS(wstatus):0;
  if (e){ fprintf(stderr,"%s:%d cg_exec_pipe()  exited, status=%d  %s\n",func,line,wstatus, cg_error_symbol(e)); perror(""); return false;}
  return true;

}

static bool cg_httpheader_load(cg_httpheader_t *h,const char *url){
  int pipefd[2],pid;
  _pipe_curl(COPY_HEADER,url,pipefd,&pid);
  cg_httpheader_read_fd(h,pipefd[0]);
  return CG_WAITPID(pid,url);
}

static int cg_load_url(char *txt, const int txt_capacity,const char *url){
  int pipefd[2],pid;
  _pipe_curl(0,url,pipefd,&pid);
  bool ok=true;
  int nread=0;
  if (pipefd[0]<=0){
    log_errno("fd<=0  url:'%s'",url);
    ok=false;
  }else{
    for(int n; txt_capacity>nread && (n=read(pipefd[0],txt+nread,txt_capacity-nread))>0;){
      nread+=n;
    }
    close(pipefd[0]);
  }
  txt[nread]=0;
  return CG_WAITPID(pid,url) && ok?nread:-1;
}



static int cg_httpheader_from_file(cg_httpheader_t *h,const char *f){
  const int fd=open(f,O_RDONLY);
  if (fd==-1) return errno;
  cg_httpheader_read_fd(h,fd);
  close(fd);
  return 0;
}







////////////////////////////////////////////////////////////////////////////////
#endif // _cg_download_file_dot_c

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
static void sleep_exit(){
  log_verbose("ps u -p %d",getpid());
  log_verbose("ls -l /proc/%d/fd",getpid());
  log_exited_function(ANSI_RED"Going to sleep"ANSI_RESET);
  usleep(1000*1000*100);
  exit(0);
}



#define cg_httpheader_print(msg,h,fo) _viamacro_cg_httpheader_print(msg,h,fo, __func__,__LINE__)
static void _viamacro_cg_httpheader_print(const char *msg,const cg_httpheader_t *h, FILE *fo, const char *func,const int line){
  if (!fo) fo=stderr;
  fprintf(fo,"%p{ lines:%d is_header:%s is_404:%s  size:%ld mtime:%ld }",h,h->lines,success_or_fail(h->is_header), yes_no(h->is_404), (long)h->size,(long)h->mtime);
}

int main(int argc, char *argv[]){
  const char *urlgz="ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.gz";
  const char *urlbz2="http://localhost/~cgille/test/t.txt.bz2";
  const char *urlxz="http://localhost/~cgille/test/t.txt.xz";
  const char *filegz="/var/lib/apt/lists/debian.charite.de_debian_dists_bookworm_main_dep11_Components-amd64.yml.gz";
  const char *url="ftp://ftp.uniprot.org/pub/databases/uniprot/LICENSE";
  const char *dst="/home/cgille/tmp/out.txt";
  //    const char *urlgz="https://files.rcsb.org/download/1SBT.pdb.gz";

  unlink(dst);
  const char *fsrc="/home/cgille/.ZIPsFS/DB/pride/DB/pride.URL";
  const char *fdst="/home/_cache/cgille/ZIPsFS/modifications/zipsfs/.preloaded_by_root/DB/pride.URL";
#define PRINT_URL_DST() fprintf(stderr,"%s -> %s\n",url,dst)
  switch(8){
  case 0:{
    bool ok=cg_copy_url_or_file(0,fsrc,fdst,NULL,NULL);
    log_msg("cg_copy_url_or_file(%d,%s,%s)  ok:%d",0,fsrc,fdst,ok);
  }
    break;
  case 1:
    cg_copy_url_or_file(0,argv[1],"/dev/stdout",NULL,NULL);
    sleep_exit();
    break;
  case 2:{
    cg_httpheader_t h={0};
    cg_httpheader_load(&h,url);
    fprintf(stderr,"url:%s\n",url);
    cg_httpheader_print("",&h,stderr);
    fprintf(stderr,"\n");
  }
    break;
  case 3:{
    cg_httpheader_t h={0};
    cg_httpheader_from_file(&h,argv[1]);
    fprintf(stderr,"file:%s\n",argv[1]);
    cg_httpheader_print("",&h,stderr);
    fprintf(stderr,"\n");
  }
    break;
  case 4:{
    int in=open(filegz,O_RDONLY);
    if (in==-1){ perror(urlgz); return 1;}
    cg_read_gzip(in,STDOUT_FILENO,NULL,NULL);
    sleep_exit();
  }
    break;
  case 5:{
    char url[PATH_MAX+1];
    cg_url_by_dot_url_file(url,argv[1],argv[2]);
    printf(" url:'%s'\n",url);
  }
    break;
  case 6:
    FOR(i,1,argc) printf("%'ld ->%'ld ",atol(argv[i]), closest_with_identical_digits(atol(argv[i])));
    break;
  case 7:
    printf("curl: %d \n",is_installed_curl());
    break;
  case 8:{
    int ret=0;
    PRINT_URL_DST();
    ret=cg_download_url(COPY_HEADER*000|COMPRESSION_bz2,urlbz2,dst,NULL,NULL);
    log_exited_function(ANSI_FG_RED"ret: %d"ANSI_RESET,ret);
  }
    break;
  case 9:{
    char txt[999];
    const int nread=cg_load_url(txt, sizeof(txt)-1,"ftp://ftp.expasy.org/databases/uniprot/current_release/relnotes.txt");
    log_msg("read: %d",nread);
    if (read>0) write(STDOUT_FILENO,txt,nread);
  }
  }



  return 0;
}

#endif //__INCLUDE_LEVEL__
#undef E


// mnt/DB/ebi/databases/pdb/data/structures/all/pdb

// zcat    mnt/DB/ebi/databases/pdb/data/structures/models/obsolete/pdb/ap/pdb1apd.ent.gz| head
