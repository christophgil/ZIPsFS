////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS          ///
/// Dynamically downloaded files ///
////////////////////////////////////
#define NET_SFX_HEADER ".HeaDeR"
#define NET_SFX_FAIL   ".FaiLeD"
#define NET_SFX_HEADER_L (sizeof(NET_SFX_HEADER)-1)
#define IN_DIR_INTERNET(vp,vp_l) (_root_writable && _root_writable && vp_l> DIR_INTERNET_L+7 && vp[DIR_ZIPsFS_L]=='/' && vp[DIR_INTERNET_L]=='/' && !strncmp(vp,DIR_INTERNET,DIR_INTERNET_L))
#define IS_DIR_INTERNET(vp,vp_l) (_root_writable && _root_writable && vp_l==DIR_INTERNET_L   && vp[DIR_ZIPsFS_L]=='/' && !strcmp(vp,DIR_INTERNET))

#define net_filepath_l(isHeader,vp,vp_l) (_root_writable->rootpath_l+vp_l+(isHeader?NET_SFX_HEADER_L:0))
#define net_filepath(rp,isHeader,vp,vp_l) stpcpy(stpcpy(stpcpy(rp,_root_writable->rootpath),vp),(isHeader?NET_SFX_HEADER:""))

#define net_is_internetfile(vp,vp_l) (net_internet_file_colon(vp,vp_l)>0)
static char *net_local_dir(const bool mkdir){
  static char d[MAX_PATHLEN+1]={0};
  if (!_root_writable){ assert(_root_n);return NULL;}
  if (!*d) net_filepath(d,false,DIR_INTERNET,DIR_INTERNET_L);
  if (mkdir && *d) cg_recursive_mkdir(d);
  return d;
}
static int net_internet_file_colon(const char *vp,const int vp_l){
  const int colon=((IN_DIR_INTERNET(vp,vp_l) && !ENDSWITH(vp,vp_l,NET_SFX_HEADER) && !ENDSWITH(vp,vp_l,NET_SFX_FAIL)) && !cg_starts_digits_char(vp,8,'_')) ? cg_str_str(1+DIR_INTERNET_L+vp,",,,") : 0;
  //log_exited_function("%s %d",vp,colon);
  return colon;
}
static void net_url_for_file(char *url, const char *vp,const int vp_l){
  const int colon=net_internet_file_colon(vp,vp_l);
  if (colon<=0){
    *url=0;
  }else{
    strcpy(url,DIR_INTERNET_L+1+vp)[colon]=':';
    for(char *s=url;*s;s++) if (*s==',') *s='/';
  }
}
static bool net_header_download(const char *rph, const bool overwrite,const char *vp,const int vp_l){
  char url[vp_l]; net_url_for_file(url,vp,vp_l);
  const bool ok=!overwrite && cg_file_exists(rph) || net_call_curl(true,url,rph);
  //log_exited_function("%s %s overwrite:%d ok:%d",vp,url,overwrite,ok);
  return ok;
}

static bool net_maybe_download(struct zippath *zpath){
  if (!net_is_internetfile(VP(),VP_L())) return false;
  const char *vp=VP();
  const int vp_l=VP_L();
  char rp[net_filepath_l(false,vp,vp_l)+1]; net_filepath(rp,false,vp,vp_l);
  char rph[net_filepath_l(true,vp,vp_l)+1]; net_filepath(rph,true,vp,vp_l);
  char url[vp_l]; net_url_for_file(url,vp,vp_l);
  struct stat st={0};
  const bool updateHeader=!stat(rph,&st) && config_internet_update_header(url,&st);
  if (!net_header_download(rph,updateHeader,vp,vp_l)) return false;
  struct stat sth={0};
  if (!net_parse_header(&sth,rph)) return false;
  if (!cg_file_exists(rp)){
    if ((stat(rp,&st) || st.st_mtime<sth.st_mtime) && !net_call_curl(false,url,rp)) return false;
    if (configuration_internet_with_date_in_filename(url)){
      const time_t t=sth.st_mtime;
      struct tm lt; gmtime_r(&t,&lt);
      char rpdate[strlen(rp)+16]; strftime(net_filepath(rpdate,false,DIR_INTERNET,DIR_INTERNET_L),15,"/%Y%m%d_",&lt);
      const char *slash=strrchr(rp,'/');    ASSERT(slash!=NULL);
      if (!slash) return false;
      strcat(rpdate,slash+1);
      link(rp,rpdate);
    }
  }
  zpath_reset_keep_VP(zpath);
  zpath->realpath=zpath_newstr(zpath);
  zpath_strcat(zpath,rp);
  ZPATH_COMMIT_HASH(zpath,realpath);
  return true;
}

static bool net_call_curl(const bool header,const char *url, const char *outfile){
  //log_entered_function("header: %d  url: %s  outfile: %s",header,url,outfile);
  net_local_dir(true);
  char tmp[MAX_PATHLEN+16];
  sprintf(tmp,"%s.%d.tmp",outfile,_pid);
  const char *cmd[]={"curl",header?"-I":"", "-o",tmp,url,(char*)0};
  const pid_t pid=fork(); /* Make real file by running external prg */
  if (pid<0){ log_errno("fork() waitpid 1 returned %d",pid);return false;}
  if (!pid){
    cg_array_remove_element(cmd,"");
    cg_log_exec_fd(STDERR_FILENO,cmd,NULL);
    execvp(cmd[0],(char *const *)cmd);
    exit(errno);
  }else{
    int status;
    waitpid(pid,&status,0);
    char errfile[strlen(outfile)+sizeof(NET_SFX_FAIL)+1];
    stpcpy(stpcpy(errfile,outfile),NET_SFX_FAIL);
    unlink(errfile);
    const bool tmpExists=cg_file_exists(tmp);
    cg_log_waitpid(!tmpExists?-2:pid,status,errfile,false,cmd,NULL);
    if (!tmpExists){
      warning(WARN_NET,tmp,"Does not exist");
    }else{
      const char *dst=outfile;
      if (ENDSWITH(outfile,strlen(outfile),NET_SFX_HEADER)){
        if (!net_parse_header(NULL,tmp)) dst=errfile;
        const int fd=open(tmp,O_RDWR|O_APPEND);
        if (fd>0){ cg_log_exec_fd(fd,cmd,NULL); close(fd);}
      }
      unlink(dst);
      return !cg_rename(tmp,dst);
    }
  }
  return false;
}

static bool net_getattr(struct stat *st, const char *vp,const int vp_l){
  if (!net_is_internetfile(vp,vp_l)) return false;
  stat_init(st,0,NULL);
  char rph[net_filepath_l(true,vp,vp_l)+1]; net_filepath(rph,true,vp,vp_l);
    return net_header_download(rph,false,vp,vp_l) && net_parse_header(st,rph);
}

static bool net_parse_header(struct stat *st,const char *rp){
  FILE *f=fopen(rp,"r");
  if (!f){ warning(WARN_NET,rp,"Cannot open"); return false; }
  char line[256];
  bool ok=false;
  while(fgets(line,sizeof(line),f)){
#define E(P,code) if (!strncmp(line,P,sizeof(P)-1)){ok=true;const int l=sizeof(P)-1;if(st){code;}}
    E("Last-Modified:",st->st_mtime=parse_http_time(line+l));
    E("Content-Length:",st->st_size=atol(line+l));
#undef E
  }
  fclose(f);
  return ok;
}

static time_t parse_http_time(const char *s){
  struct tm g={0};
  char M[4];
  const int n=sscanf(s," %*[a-zA-Z,] %d %3s %d %d:%d:%d",&g.tm_mday,M, &g.tm_year, &g.tm_hour, &g.tm_min, &g.tm_sec);
  if (n!=6){ warning(WARN_NET,s,"Parsed only %d fields",n); return 0;}
  const int hit=cg_str_str("JanFebMarAprMayJunJulAugSepOctNovDec",M);
  if (hit<0){ warning(WARN_NET,s,"Cannot parse month"); return 0;}
  g.tm_mon=hit/3;
  g.tm_year-=1900;
  return timegm(&g);
}


static int net_filename_from_header_file(char *n,const int n_l){
  if (!ENDSWITH(n,n_l,NET_SFX_HEADER)) return 0;
  n[n_l-NET_SFX_HEADER_L]=0;
  return n_l-NET_SFX_HEADER_L;
}

//curl -o t.txt  -I https://ftp.uniprot.org/pub/databases/uniprot/current_release/knowledgebase/complete/uniprot_sprot.fasta.gz
/*
  m=~/tmp/ZIPsFS/mnt
  n=$m/ZIPsFS/n
  h=$n/https,,,ftp.uniprot.org,pub,databases,uniprot,README
  f=$n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE
*/
//curl -o t.txt  -I https://files.rcsb.org/download/1SBT.cif.gz
//curl -o t.txt  -I https://ftp.uniprot.org/pub/databases/uniprot/README
//Content-Length: 93100075
//Last-Modified: Wed, 18 Jun 2025 23:00:00 GMT
