////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS          ///
/// Dynamically downloaded files ///
////////////////////////////////////
// cat mnt/ZIPsFS/n/gz/https,,,files.rcsb.org,download,1SBT.pdb

_Static_assert(WITH_INTERNET_DOWNLOAD,"");
#define NET_SFX_HEADER ".HeaDeR"
#define NET_SFX_FAIL   ".FaiLeD"
#define NET_SFX_HEADER_L (sizeof(NET_SFX_HEADER)-1)

#define MAX_LEN_COMPRESS_EXT 5
#define net_estimate_filepath_l(vp,vp_l) (_root_writable->rootpath_l+vp_l+(NET_SFX_HEADER_L+MAX_LEN_COMPRESS_EXT+1))
#define net_filepath(rp,vp,vp_l) stpcpy(stpcpy(rp,_root_writable->rootpath),vp)
#define net_filepath_header(rp,vp,vp_l,ext) stpcpy(stpcpy(stpcpy(stpcpy(rp,_root_writable->rootpath),vp),ext),NET_SFX_HEADER)
#define net_is_internetfile(vp,vp_l) (net_internet_file_colon(vp,vp_l)>0)


static int net_internet_file_colon(const char *vp,const int vp_l){
  const int colon=((vp[DIR_INTERNET_L]=='/' && !ENDSWITH(vp,vp_l,NET_SFX_HEADER) && !ENDSWITH(vp,vp_l,NET_SFX_FAIL)) && !cg_starts_digits_char(vp+DIR_INTERNET_L+1,8,'_')) ? cg_str_str(1+DIR_INTERNET_L+vp,",,,") : 0;
  //log_exited_function("%s %d",vp,colon);
  return colon;
}
static void net_url_for_file(char *url, const char *vp,const int vp_l, const char *ext){
  const int colon=net_internet_file_colon(vp,vp_l);
  if (colon<=0){
    *url=0;
  }else{
    stpcpy(stpcpy(url,cg_last_slash_l(vp,vp_l)+1+vp),ext);
    url[colon]=':';
    for(char *s=url;*s;s++) if (*s==',') *s='/';
  }
  //log_exited_function("url=%s",url);
}
static bool net_header_download(const char *rph, const bool overwrite,const char *vp,const int vp_l,const char *ext){
  char url[vp_l]; net_url_for_file(url,vp,vp_l,ext);
  const bool ok=!overwrite && cg_file_size(rph)>=0 || cg_download_url(COPY_HEADER,url,rph,NULL,NULL);
  //log_exited_function("%s %s overwrite:%d ok:%d",vp,url,overwrite,ok);
  return ok;
}



// http://ftp.gnu.org/gnu/gawk/gawk-4.0.2.tar.xz
// http://ftp.gnu.org/gnu/binutils/binutils-2.23.1.tar.bz2

#define _INTERNET_COMPRESSION_LOOP(vp,vp_l)  const int compress_from_name=cg_compression_for_filename(vp,vp_l);   int mask=0;\
  FOR(iCompress,0,(compress_from_name?1:COMPRESSION_NUM))\
    if (!iCompress || ((1<<iCompress)|(iCompress==1?(mask=config_internet_try_compressed(vp,vp_l)):mask)))

static bool net_maybe_download(zpath_t *zpath){
  if (!net_is_internetfile(VP(),VP_L())) return false;
  const char *vp=VP();
  const int vp_l=VP_L();
  struct stat st={0};
  char url[vp_l];
  char rph[net_estimate_filepath_l(vp,vp_l)];
  char rp[net_estimate_filepath_l(vp,vp_l)];
  _INTERNET_COMPRESSION_LOOP(vp,vp_l){


    //if (iCompress==1) mask=config_internet_try_compressed(vp,vp_l);
    //if (iCompress && !((1<<iCompress)|mask)) continue;

    const char *ext=cg_compression_file_ext(iCompress,NULL);
    net_filepath(rp,vp,vp_l);
    net_filepath_header(rph,vp,vp_l,ext);
    net_url_for_file(url,vp,vp_l,ext);
    const bool updateHeader=!stat(rph,&st) && config_internet_update_header(url,&st);
    if (!net_header_download(rph,updateHeader,vp,vp_l,ext)) continue;
    struct stat sth={0};
    if (!net_parse_header(&sth,rph,iCompress)) continue;
    if ((stat(rp,&st) || st.st_mtime<sth.st_mtime) && !cg_download_url(iCompress,url,rp, root_loading_active,zpath->root)) continue;
    if (config_internet_hardlink_with_date_in_filename(url)){
      const time_t t=sth.st_mtime;
      struct tm lt; gmtime_r(&t,&lt);
      char rpdate[strlen(rp)+16]; strftime(net_filepath(rpdate,DIR_INTERNET,DIR_INTERNET_L),15,"/%Y%m%d_",&lt);
      const char *slash=strrchr(rp,'/');    ASSERT(slash!=NULL);
      if (!slash) return false;
      strcat(rpdate,slash+1);
      link(rp,rpdate);
    }
    zpath_reset_keep_VP(zpath);
    zpath->realpath=zpath_newstr(zpath);
    zpath_strcat(zpath,rp);
    ZPATH_COMMIT_HASH(zpath,realpath);
    return true;
  }
  //log_debug_now(RED_FAIL"rp: %s",rp);
  return false;
}

static bool net_getattr(struct stat *st, const virtualpath_t *vipa){
  //log_entered_function("vp:%s  %d",vipa->vp,net_is_internetfile(vipa->vp,vipa->vp_l));
  if (!net_is_internetfile(vipa->vp,vipa->vp_l)) return false;

  _INTERNET_COMPRESSION_LOOP(vipa->vp,vipa->vp_l){
    const char *ext=cg_compression_file_ext(iCompress,NULL);
    stat_init(st,0,NULL);
    char rph[net_estimate_filepath_l(vipa->vp,vipa->vp_l)];
    net_filepath_header(rph,vipa->vp,vipa->vp_l,ext);
    //log_debug_now("rph: %s  ext:%s",rph,ext);
    if (net_header_download(rph,false,vipa->vp,vipa->vp_l,ext) && net_parse_header(st,rph,iCompress)) return true;
  }
  return false;
}

static bool net_parse_header(struct stat *st,const char *rp,const int iCompress){
  //log_entered_function("rp:%s",rp);
  FILE *f=fopen(rp,"r");
  if (!f){ warning(WARN_NET,rp,"Cannot open"); return false; }
  char line[256];
  bool ok=false,notOK=false,http404=false;
  int iLine=0;
  while(fgets(line,sizeof(line),f)){
    if (!*line) continue;
    iLine++;
    /* Header field names are case-insensitive. */
    // cppcheck-suppress-macro unreadVariable
    #define I(P) if (!strncasecmp(line,P,sizeof(P)-1))
#define E(P,code) I(P) {ok=true;const int l=sizeof(P)-1;if(st){code;}}
    I("HTTP"){
      const char *spc=strchr(line,' ');
      if (spc){
        const int code=atoi(spc+1);
        if (code==404) http404=true;
      }
    }
    E("Last-Modified:",st->st_mtime=parse_http_time(line+l));
    E("Content-Length:",st->st_size=atol(line+l));
    if (iCompress) st->st_size=closest_with_identical_digits(10*st->st_size); /* Guess file size and make it a Schnapszahl */
    E("Content-type:",);
    I("Content-Location:") notOK=true; /* http://hgdownload.soe.ucsc.edu/goldenPath/archive/susScr3/uniprot/2022_05/UP000005640_9606.fasta.gz without gz */
#undef E
#undef I
  }
  fclose(f);
  if (!ok){
    warning(WARN_NET,rp,"HTTP-header neither contains field Last-Modified nor Content-Length nor Content-type");
  }else if(st && !st->st_size){
    warning(WARN_NET,rp,"HTTP-header missing Content-Length:");
    //cg_print_stacktrace(0);
    st->st_size=1<<30;
  }
  if (!iLine || http404){
    log_verbose("Going to remove '%s' because %s",rp,!iLine?" it is empty":" HTTP 404");
    cg_unlink(rp);
    return false;
  }
  return ok && !notOK;
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



/* static int net_filename_from_header_file(char *n,const int n_l){ */
/*   if (!ENDSWITH(n,n_l,NET_SFX_HEADER)) return 0; */
/*   n[n_l-NET_SFX_HEADER_L]=0; */
/*   return n_l-NET_SFX_HEADER_L; */
/* } */
#undef X
// ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml.gz

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
// HTTP/2 200
