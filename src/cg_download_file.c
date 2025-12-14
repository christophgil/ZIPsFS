//////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                        ///
/// Logging in  ZIPsFS                                         ///
/// This file provides                                         ///
///   cg_copy_url_or_file() Needed by ZIPsFS_preloadfiledisk.c ///
///   cg_download_url() Needed by ZIPsFS_internet.c            ///
//////////////////////////////////////////////////////////////////

#ifndef _cg_download_file_dot_c

#include "cg_utils.c"
#include <zlib.h>

#define E(msg) perror(__FILE__":"STRINGIZE(__LINE__)" "msg);


static int cg_read_gzip(const int in, const int out){
  errno=0;
  gzFile gz=gzdopen(in,"rb");
  if (!gz) {
    fprintf(stderr, "gzdopen failed\n");
    close(in);
    return -1;
  }
  char buf[16*1024];
  long count=0;
  int n;
  while((n=gzread(gz,buf,sizeof(buf)-1))>0){
    cg_fd_write(out,buf,n);
    count+=n;
  }
  gzclose(gz); /* Do not close close(in) !!! */
  return n<0?-1:count;
}


static long cg_read_fd(const int in, const int out){
  errno=0;
  char buf[16*1024];
  int n;
  long count=0;
  while ((n=read(in,buf,sizeof(buf)-1))>0){ cg_fd_write(out,buf,n); count+=n;}
  close(in);
  return n<0?-1:count;
}

#define COPY_GUNZIP              (1<<1)
#define COPY_APPEND_GZ           (1<<3)
#define COPY_HEADER              (1<<4)
#define COPY_NO_CURL             (1<<5)
#define COPY_NO_WGET             (1<<6)
#define COPY_USE_DOT_URL_FILE    (1<<8)
#define COPY_ADD_STDERR          (1<<9)




static bool cg_exec_write_to(const int opt,const char *cmd[], const char *env[], const char *outfile){
  errno=0;
  log_entered_function("cmd:%s   outfile:%s",*cmd,outfile);
  cg_recursive_mk_parentdir(outfile);
  unlink(outfile);
  if (cg_file_exists(outfile)) log_errno("Failed to remove %s",outfile);
  errno=0;
  int out=open(outfile,O_WRONLY|O_CREAT|O_TRUNC,0644);
  if (out==-1) { log_errno("open(%s)",outfile); return false;}
  bool ok=false;
  if (opt&COPY_GUNZIP){
    int pipefd[2]={0};
    if (pipe(pipefd)==-1){ E("pipe");return false;}
    const pid_t pid=fork();
    if (pid<0){ E("fork"); return false;}
    if (!pid){
      close(pipefd[1]);
      const long n=cg_read_gzip(pipefd[0],out);
      exit(n>0?0:errno);
    }
    close(pipefd[0]);
    ok=cg_fork_exec(cmd,env,pipefd[1],(opt&COPY_ADD_STDERR)?pipefd[1]:0);
    close(pipefd[1]);
  } else{
    ok=cg_fork_exec(cmd,env,out,(opt&COPY_ADD_STDERR)?out:0);
  }
  close(out);
  log_exited_function("%s  outfile:%s  exists:%d %s",*cmd,outfile,cg_file_exists(outfile),success_or_fail(ok));
  if (!ok){
    log_errno("%s",cmd[0]);
    return false;
  }
  return cg_file_exists(outfile);
}
/*************************/
/* Downloading / Copying */
/*************************/

static bool cg_download_url(int opt, const char *url, const char *outfile){
  bool ok=false;
  const char *cmd[9];
  RLOOP(i,2){
    memset(cmd,0,sizeof(cmd));
    const bool curl=i==is_installed_curl();
    int k=0;
#define P cmd[k++]
    bool is_stderr=false;
    if (curl){
      if ((opt&COPY_NO_CURL) || !is_installed_curl()) continue;
      P="curl"; P="-o"; P="-"; if (opt&COPY_HEADER) P="-I";
    }else{
      if ((opt&COPY_NO_WGET) || !is_installed_wget() || DEBUG_NOW==DEBUG_NOW) continue;

      // --spider https://www.zyxware.com/articles/2402/viewing-http-headers-using-wget

      P="wget"; P="-O";
      if (opt&COPY_HEADER){
        P="/dev/null"; P="-S"; P="-q";
        is_stderr=true;
      }else{
        P="-";
      }
    }
    P=url; P=(char*)0;
#undef P
    TMP_FOR_FILE(tmp,outfile);
    unlink(tmp);
    if (cg_exec_write_to((is_stderr?COPY_ADD_STDERR:0)|(opt&COPY_GUNZIP),cmd,NULL,tmp) && cg_file_size(tmp)>0){
      if (rename(tmp,outfile)){
        log_errno("rename %s ->%s",tmp,outfile);
      }else{
        ok=true;
      }
    }else ok=false;
    unlink(tmp);
    if (ok) break;
  }
  log_exited_function("url: %s  outfile: %s  ok: %d  file_size:%ld",url,outfile,ok,cg_file_size(outfile));
  return ok && cg_file_size(outfile)>0;
}



static int cg_read_file(char *buf, const int buf_l, const char *path){
  const int fd=open(path,O_RDONLY);
  if (fd<2) return -1;
  int pos=0,n;
  while (buf_l>pos && (n=read(fd,buf+pos,buf_l-pos))>0) pos+=n;
  close(fd);
  return pos;
}

/*******************************************************************************/
/* Unless the local file dst exists, it will be downloaded. */
/* dst will contain the downloaded path.                                       */
/* If parameter zpath is NULL, it will be obtained from parameter d.           */
/*******************************************************************************/
static bool cg_url_by_dot_url_file(char url[PATH_MAX+1], const char *root_or_null, const char *vp){
  const int vp_l=strlen(vp),root_l=cg_strlen(root_or_null);
  char tmp[root_l+vp_l+6];
  const int tmp_l=stpcpy(stpcpy(tmp,root_or_null?root_or_null:""),vp)-tmp;
  tmp[tmp_l]=0;
  for(int i=tmp_l+1; --i>=root_l;){
    if (i!=tmp_l && tmp[i]!='/') continue;
    stpcpy(tmp+i,".URL");
    int url_l=cg_read_file(url,MAX_PATHLEN,tmp);
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


static bool cg_url_associated_with_path(char url[PATH_MAX], const char *path){
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
/* Entry point for ZIPsFS_preloadfiledisk.c  */
/*********************************************/
static bool cg_copy_url_or_file(const int opt,const char *source, const char *dst){
  assert(!(opt&~(COPY_GUNZIP)));
  char src[PATH_MAX+1];
  if (!realpath(source,src)){ log_errno("%s",source); return false;}
  log_debug_now("realpath: %s",src);
  char url[PATH_MAX+1];
  cg_url_associated_with_path(url,source);
  if (*url){
    return cg_download_url(opt,url,dst);
  }
  bool ok=false;
  errno=0;
  TMP_FOR_FILE(tmp,dst);
  const int out=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,0644);
  if (out<3){ log_errno("open(%s)",tmp); return false; }
  const int in=open(source,O_RDONLY);
  if (in<3){ log_errno("open(%s)",tmp); return false; }
  if (opt&COPY_GUNZIP){
    ok=cg_read_gzip(in,out)>=0;
  }else{
    ok=cg_read_fd(in,out)>=0;
  }
  if (ok){
    if (cg_file_size(tmp)<=0){ unlink(tmp); ok=false;}
    if (rename(tmp,dst)){ log_errno("rename %s ->%s",tmp,dst); ok=false; }
  }
  return ok && cg_file_exists(dst);
}



#endif // _cg_download_file_dot_c



#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0





static void sleep_exit(){
  log_verbose("ps u -p %d",getpid());
  log_verbose("ls -l /proc/%d/fd",getpid());
  log_exited_function(ANSI_RED"Going to sleep"ANSI_RESET);
  usleep(1000*1000*100);
  exit(0);
}






int main(int argc, char *argv[]){
  const char *urlgz="ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.gz";
  const char *filegz="/var/lib/apt/lists/debian.charite.de_debian_dists_bookworm_main_dep11_Components-amd64.yml.gz";
  if (0){
    test_from_man();
    sleep_exit();
  }

  if (0){
    const char *cmd[]={"cat",filegz,NULL};
    int ret=cg_exec_write_to(COPY_GUNZIP,cmd,NULL, "/dev/stdout");
    log_debug_now("rrrrrrrrrrrrrrrrrrrrrr ret: %d",ret);
    return 1;
  }
  if (0){
    const char *cmd[]={"seq","10",NULL};
    int ret=cg_exec_write_to(0,cmd,NULL, "/dev/stdout");
    log_debug_now("rrrrrrrrrrrrrrrrrr ret: %d",ret);
    return 1;
  }

  if (0){

    int in=open(filegz,O_RDONLY);
    if (in==-1){ perror(urlgz); return 1;}
    cg_read_gzip(in,STDOUT_FILENO);
    sleep_exit();
  }


  if (0){
    char url[PATH_MAX+1];
    cg_url_by_dot_url_file(url,argv[1],argv[2]);
    printf(" url:'%s'\n",url);
    exit(9);
  }
  if (0){
    long n=atol(argv[1]);
    printf("%'ld ->%'ld ",n, closest_with_identical_digits(n));
    exit(9);
  }

  if (0){
    printf("curl: %d  wget: %d\n",is_installed_curl(),is_installed_wget());
    printf("curl: %d  wget: %d\n",is_installed_curl(),is_installed_wget());
    exit(0);
  }
  {
    //    const char *url="https://XXXfiles.rcsb.org/download/1SBT.pdb";
    const char *url="ftp://ftp.uniprot.org/pub/databases/uniprot/LICENSE";
    const char *dst="/home/cgille/tmp/out.txt";
    //    const char *urlgz="https://files.rcsb.org/download/1SBT.pdb.gz";

    if(0){
      int ret=0;
      //      F=~/tmp/out.txt; rm -f $F;  cg_utils;   ls -l $F ; strings $F | head -n 4
      //      cg_download_url(COPY_HEADER|COPY_NO_CURL,url,dst);
      //cg_download_url(COPY_HEADER,url,dst);
      //ret=cg_download_url(0,url,dst)

      //ret=cg_download_url_try_gunzip_atomic(url,dst);
      ret=cg_download_url(0,url,dst);
      log_exited_function(ANSI_FG_RED"ret: %d"ANSI_RESET,ret);
      exit(0);
    }
    if (1){
      int opt=0;
      const char *cmd[]={"curl","-o","-",urlgz,NULL}; ; opt=COPY_GUNZIP;
      //      const char *cmd[]={"curl","-o","-","ftp://ftp.uniprot.org/pub/databases/uniprot/LICENSE",NULL};

      cg_exec_write_to(opt,cmd,NULL, "/dev/stdout");
      //my_leak("/dev/stdout");


      sleep_exit();
    }
  }



  return 0;
}

#endif //__INCLUDE_LEVEL__
#undef E
