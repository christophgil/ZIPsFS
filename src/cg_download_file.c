//////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                        ///
/// Logging in  ZIPsFS                                         ///
/// This file provides                                         ///
///   cg_copy_url_or_file() Needed by ZIPsFS_preloaddisk.c ///
///   cg_download_url() Needed by ZIPsFS_internet.c            ///
//////////////////////////////////////////////////////////////////

#ifndef _cg_download_file_dot_c
#include "cg_utils.c"
#include "cg_exec_pipe.c"

// /usr/include/zlib.h
#define E(msg) perror(__FILE__":"STRINGIZE(__LINE__)" "msg);






static int cg_read_file_into_buffer(char *buf, const int buf_l, const char *path){
  const int fd=open(path,O_RDONLY);
  if (fd<2) return -1;
  int pos=0,n;
  while (buf_l>pos && (n=read(fd,buf+pos,buf_l-pos))>0) pos+=n;
  close(fd);
  return pos;
}


static long cg_read_fd(const int in, const int out,void (*progress)(void*), void *progress_para){
  errno=0;
  char buf[16*1024];
  int n;
  long count=0;
  for (int i=0; (n=read(in,buf,sizeof(buf)-1))>0; i++){
    if (progress) progress(progress_para);
    cg_fd_write(out,buf,n);
    count+=n;
    if (DEBUG_THROTTLE_DOWNLOAD){
      //if (!(i%16))
      fputc('l',stderr);
      usleep(10*1000);
    }
  }
  close(in);
  return n<0?-1:count;
}



#define COPY_FLAGS_BEGIN         (1<<14)
#define COPY_HEADER              (1<<14)
#define COPY_USE_DOT_URL_FILE    (1<<15)
#define COPY_ADD_STDERR          (1<<16)
_Static_assert(!(COMPRESSION_MASK&COPY_FLAGS_BEGIN),"COMPRESSION_MASK");

#define OPT_AND_COMPRESS()  const int opt=opt_and_compress&~COMPRESSION_MASK, iCompress=opt_and_compress&COMPRESSION_MASK

/*************************/
/* Downloading / Copying */
/* Entry point for ZIPsFS_internet.c */
/*************************/
static bool cg_download_url(const int opt_and_compress, const char *url, const char *outfile, void (*progress)(void*), void *progress_para){
  if (!outfile) return false;
  OPT_AND_COMPRESS();
#define _CG_DOWNLOAD_URL_CMD 16
  const char *cmd[_CG_DOWNLOAD_URL_CMD];
  if (!is_installed_curl()){ log_error("curl is not installed.");  return false;}
  {
#define P(a) cmd[k++]=a;
    int k=0;
    P("curl")P("-o")P("-");
    if (opt&COPY_HEADER) P("-I");
#ifdef CURL_OPTS
    const char *extra[]={CURL_OPTS NULL};
    for(const char **p=extra; *p; p++) P(*p);
#endif //CURL_OPTS
    P(url)P(NULL)assert(k<_CG_DOWNLOAD_URL_CMD);
#undef P
  }
  TMP_FOR_FILE(tmp,outfile);

  //log_debug_now("url:%s iCompress:%d",url,iCompress);
  const int fdout=open(outfile,O_WRONLY|O_CREAT|O_TRUNC,0644);
  if (fdout==-1){ log_errno("open(%s)",outfile); return false;}
  bool ok=true;
  if (iCompress){
    const char *decomp[DECOMPRESS_CMD_MAX+1];  *decomp=0;
    if (iCompress!=COMPRESSION_GZ) cg_cmd_decompress(iCompress,decomp,NULL);
    ok=cg_exec_pipe(cmd,NULL,*decomp?decomp:_CMD_ZLIB,fdout,progress,progress_para);
  }else{
    ok=cg_fork_exec(cmd,NULL,0,fdout,0);
  }
  close(fdout);
  //log_exited_function("%s  outfile:%s  exists:%d %s",*cmd,outfile,cg_file_exists(outfile),success_or_fail(ok));
  if (!ok){ log_errno("curl  decompress: '%s'",cg_compression_file_ext(iCompress,NULL));  return false; }
  //log_exited_function("cg_fork_exec returns %d  outfile: %s  %ld bytes exists: %d",ok,outfile,cg_file_size(outfile), cg_file_exists(outfile));
  return cg_file_exists(outfile);
}


/*******************************************************************************/
/* Unless the local file dst exists, it will be downloaded. */
/* dst will contain the downloaded path.                                       */
/* If parameter zpath is NULL, it will be obtained from parameter d.           */
/*******************************************************************************/


/******************************************************/
/* Has a path component a corresponding dot-URL file? */
/******************************************************/
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
  if (out<3){ log_errno("open(%s)",tmp); return false; }
  const char *cmd[DECOMPRESS_CMD_MAX+1];
  bool ok=cg_cmd_decompress(iCompress,cmd,src);
  if (ok){
    ok=cg_fork_exec(cmd,NULL,0,out,0);
  }else{
    const int in=open(src,O_RDONLY);
    if (in<3){ log_errno("open(%s)",tmp);  return false; }
    ok=(iCompress==COMPRESSION_GZ?
        cg_read_gzip(in,out,progress,progress_para):
        cg_read_fd(  in,out,progress,progress_para))>0;
  }
  return ok && cg_rename_tmp_outfile(tmp,dst);
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






int main(int argc, char *argv[]){
  const char *urlgz="ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.gz";
  const char *filegz="/var/lib/apt/lists/debian.charite.de_debian_dists_bookworm_main_dep11_Components-amd64.yml.gz";
  if (1){
    const char *cmd[]={"cat",filegz,NULL};
    cg_exec_write_to(COMPRESSION_GZ,cmd, NULL, "/home/cgille/tmp/test_out", NULL,NULL);
    exit(1);
  }



  if (1){
    const char *src="/home/cgille/.ZIPsFS/DB/pride/DB/pride.URL";
    const char *dst="/home/_cache/cgille/ZIPsFS/modifications/ZIPsFS/.preloaded_by_root/DB/pride.URL";

    bool ok=cg_copy_url_or_file(0,src,dst,NULL,NULL);
    log_msg("cg_copy_url_or_file(%d,%s,%s)  ok:%d",0,src,dst,ok);
    exit(1);

  }
  if (0){
    cg_copy_url_or_file(0,argv[1],"/dev/stdout",NULL,NULL);
    sleep_exit();
    exit(1);
  }

  if (0){
    const char *cmd[]={"cat",filegz,NULL};
    int ret=cg_exec_write_to(COMPRESSION_GZ,cmd,NULL, "/dev/stdout",NULL,NULL);
    return 1;
  }
  if (0){
    const char *cmd[]={"seq","10",NULL};
    int ret=cg_exec_write_to(0,cmd,NULL, "/dev/stdout",NULL,NULL);
    return 1;
  }

  if (0){

    int in=open(filegz,O_RDONLY);
    if (in==-1){ perror(urlgz); return 1;}
    cg_read_gzip(in,STDOUT_FILENO,NULL,NULL);
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
    printf("curl: %d \n",is_installed_curl());
    exit(0);
  } // is_installed_wget
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
      ret=cg_download_url(0,url,dst,NULL,NULL);
      log_exited_function(ANSI_FG_RED"ret: %d"ANSI_RESET,ret);
      exit(0);
    }
    if (1){
      int opt=0;
      const char *cmd[]={"curl","-o","-",urlgz,NULL}; opt=COMPRESSION_GZ;
      cg_exec_write_to(opt,cmd,NULL, "/dev/stdout",NULL,NULL);
      sleep_exit();
    }
  }



  return 0;
}

#endif //__INCLUDE_LEVEL__
#undef E


// mnt/DB/ebi/databases/pdb/data/structures/all/pdb

// zcat    mnt/DB/ebi/databases/pdb/data/structures/models/obsolete/pdb/ap/pdb1apd.ent.gz| head
