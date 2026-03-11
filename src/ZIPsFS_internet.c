////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS          ///
/// Dynamically downloaded files ///
////////////////////////////////////
// cat mnt/ZIPsFS/n/gz/https,,,files.rcsb.org,download,1SBT.pdb

_Static_assert(WITH_INTERNET_DOWNLOAD,"");
#define NET_SFX_HEADER ".HeaDeR.TXT"
#define NET_SFX_FAIL   ".FaiLeD.TXT"
#define NET_SFX_HEADER_L (sizeof(NET_SFX_HEADER)-1)

#define MAX_LEN_COMPRESS_EXT 5
#define net_estimate_filepath_l(vp,vp_l) (_writable_path_l+vp_l+(NET_SFX_HEADER_L+MAX_LEN_COMPRESS_EXT+1))
#define net_filepath(rp,vp,vp_l) stpcpy(stpcpy(rp,_writable_path),vp)
#define net_filepath_header(rp,vp,vp_l,ext) stpcpy(stpcpy(stpcpy(stpcpy(rp,_writable_path),vp),ext),NET_SFX_HEADER)
#define net_is_internetfile(vp,vp_l) (net_internet_file_colon(vp,vp_l)>0)

#define _NET_FOREACH_COMPRESSION_LOOP(i,vp,vp_l)   if (url_e) FOR(i,0,(compress_id?1:COMPRESSION_NUM)) if (!i || ((1<<i)&(i==1?(mask=config_internet_try_compressed(vp,vp_l)):mask)))


#define _NET_FOREACH_COMPRESSION_PREAMBLE(vp,vp_l)  const int compress_id=cg_compression_for_filename(vp,vp_l);   int mask=0; char *url_e=net_url_for_file(url,vp,vp_l)

#define _NET_DL_AS_FILE        (1<<1)
#define _NET_DL_EVEN_IF_EXISTS (1<<2)

static void zpath_set_realpath_root_writable(zpath_t *zpath){
  zpath->root=_root_writable;
  zpath_set_realpath(zpath,_writable_path,VP());
  zpath_stat(0,zpath);
}

static int net_internet_filename_colon(const char *name,const int name_l){
  const int colon= !ENDSWITH(name,name_l,NET_SFX_HEADER) && !ENDSWITH(name,name_l,NET_SFX_FAIL) && !cg_starts_digits_char(name,8,'_') ? cg_str_str(name,",,,") : 0;
  return colon<0?0:colon;
}
static int net_internet_file_colon(const char *vp,const int vp_l){
  //  const int colon=((vp[DIR_INTERNET_L]=='/' && !ENDSWITH(vp,vp_l,NET_SFX_HEADER) && !ENDSWITH(vp,vp_l,NET_SFX_FAIL)) && !cg_starts_digits_char(vp+DIR_INTERNET_L+1,8,'_')) ? cg_str_str(1+DIR_INTERNET_L+vp,",,,") : 0;
  if (vp_l<DIR_INTERNET_L+8 || vp[DIR_INTERNET_L]!='/') return 0;
  int colon=net_internet_filename_colon(vp+(DIR_INTERNET_L+1),vp_l-(DIR_INTERNET_L+1));
  if (colon) colon+=DIR_INTERNET_L+1;
  //log_exited_function("vp:%s  colon:%d",vp,colon);
  return colon;
}
static char *net_url_for_file(char *url, const char *vp,const int vp_l){
  *url=0;
  const int slash1=cg_last_slash_l(vp,vp_l)+1;
  ASSERT(slash1>0);
  if (slash1<=0) return NULL;
  const int colon=net_internet_filename_colon(vp+slash1,vp_l-slash1);
  if (colon<=0) return NULL;
  char *url_e=stpcpy(url,vp+slash1);
  ASSERT(url[colon]==',');
  url[colon]=':';
  for(char *s=url;*s;s++) if (*s==',') *s='/';
  return url_e;
}

// http://ftp.gnu.org/gnu/gawk/gawk-4.0.2.tar.xz
// http://ftp.gnu.org/gnu/binutils/binutils-2.23.1.tar.bz2


static bool net_maybe_download(const int opt,zpath_t *zpath){
  if (!_writable_path_l) return false;
  const bool already=!(opt&_NET_DL_EVEN_IF_EXISTS) && find_realpath_for_root(0,zpath,_root_writable);
  char ok=false;
  cg_httpheader_t h={0};
  if (!already){
	char url[VP_L()+1];
	if (!net_maybe_download_header(opt|_NET_DL_AS_FILE,&h,url,VP(),VP_L())){
	  warning(WARN_NET,url,"Failed download header");
	  return false;
	}
	//    zpath_set_realpath(zpath,_writable_path,VP());
	zpath_set_realpath_root_writable(zpath);
	const char *ext=cg_compression_file_ext(h.compress_sfx,NULL);
	stpcpy(net_url_for_file(url,VP(),VP_L()),ext);
	ok=cg_download_url(h.compress_sfx,url,RP(), NULL,NULL);
	if (ok){
	  cg_file_set_mtime(RP(),h.mtime);
	  url[strlen(url)-cg_strlen(ext)]=0; /* url without .gz */
	  char lnk[PATH_MAX+1]; *lnk=0;
	  net_mk_hardlink(true,lnk,url,h.mtime,RP());
	}
  }
  //log_exited_function("vp:%s VP() ok:%s url:%s  compress_sfx:%d",VP(),already?GREEN_ALREADY_EXISTS:success_or_fail(ok), url,h.compress_sfx);

  return ok;
}



static bool net_maybe_download_header(const int opt, cg_httpheader_t *h, char *url, const char *vp, const int vp_l){
  _NET_FOREACH_COMPRESSION_PREAMBLE(vp,vp_l);
  char rph_buf[(opt&_NET_DL_AS_FILE)?net_estimate_filepath_l(vp,vp_l):0];
  char *rph=(opt&_NET_DL_AS_FILE)?rph_buf:NULL;
  const bool asFileUnlessExists=(opt&(_NET_DL_AS_FILE|_NET_DL_EVEN_IF_EXISTS))==_NET_DL_AS_FILE;
  /* Check whether http header had already been downloaded as file */
  bool ok=false;
  if (asFileUnlessExists  && !compress_id){ /* If no compress suffix */
	FOR(iCompress,1,COMPRESSION_NUM){
	  net_filepath_header(rph,vp,vp_l,cg_compression_file_ext(iCompress,NULL));
	  if ((ok=cg_is_executable(rph) && net_parse_header(h,rph,iCompress))){ /*  Header files with compress suffix  can serve only if executable flag. */
		h->compress_sfx=iCompress;
		break;
	  }
	}
  }
  /* Download http header, try compressen suffixes */
  if (!ok){
	_NET_FOREACH_COMPRESSION_LOOP(iCompress,vp,vp_l){
	  const char *ext=cg_compression_file_ext(iCompress,NULL);
	  if (rph) net_filepath_header(rph,vp,vp_l,ext);
	  strcpy(url_e,ext);
	  if (!rph && cg_httpheader_load(h,url)) return true;
	  if (asFileUnlessExists && cg_file_size(rph)>0 || cg_download_url(COPY_HEADER,url,rph,NULL,rph?NULL:h)){
		if (!cg_file_exists(rph)){
		  warning(WARN_NET,rph,"File does not exist");
		}else{
		  ok=net_parse_header(h,rph,iCompress);
		  if (!ok){
			warning(WARN_NET,rph,"Parsing header failed");
		  }else{
			if (iCompress) cg_set_executable(rph);
			h->compress_sfx=iCompress;
			//log_exited_function("url:%s iCompress:%d ",url,iCompress); cg_httpheader_print(h,stderr);fputs("\n\n",stderr);
			break;
		  }
		}
	  }
	}
  }
  return ok;
}
static bool net_getattr(struct stat *st, const virtualpath_t *vipa){
  if (vipa->dir==DIR_INTERNET_UPDATE && ENDSWITH(vipa->vp,vipa->vp_l,NET_SFX_UPDATE)){
	NEW_ZIPPATH(vipa);
	if (test_realpath(0,0,zpath,_root_writable)){
	  *st=zpath->stat_rp;
	  return true;
	}
	return false;
  }
  if (!net_is_internetfile(vipa->vp,vipa->vp_l)) return false;
  cg_httpheader_t h={0};
  char url[vipa->vp_l];
  const bool ok=net_maybe_download_header(_NET_DL_AS_FILE,&h,url,vipa->vp,vipa->vp_l);
  if (ok){
	stat_init(st,h.size,NULL);
	st->st_mtime=h.mtime;
	st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
  }
  return ok;
}

static bool net_parse_header(cg_httpheader_t *h,const char *rp,const int iCompress){
  const int fd=open(rp,O_RDONLY);
  const bool ok=fd>0 && !cg_httpheader_read_fd(h,fd);
  close(fd);
  if (!ok){ warning(WARN_NET|WARN_FLAG_ERRNO,rp,"Cannot open fd:%d",fd); return false; }
  //log_debug_now("h:"); cg_httpheader_print(h,stderr);fputs("\n\n",stderr);
  const char *whatErr=!h->lines?" it is empty":!h->is_header?"No header lines":h->is_404?" HTTP 404":NULL;
  if (whatErr){
	log_verbose("Going to remove '%s' because %s",rp,whatErr);
	cg_unlink(rp);
	return false;
  }
  if (iCompress && h->size) h->size=closest_with_identical_digits(h->size*100);
  if (!h->is_header){
	warning(WARN_NET,rp,"HTTP-header neither contains field Last-Modified nor Content-Length nor Content-type");
  }else if(!h->size){
	warning(WARN_NET,rp,"HTTP-header missing Content-Length:");
	h->size=closest_with_identical_digits(1<<30);
  }
  return true;
}



static bool net_update(fHandle_t *d){
  zpath_t *zpath=&d->zpath;
  char url[VP_L()];
  cg_httpheader_t h={0};
  const bool gotHeader=net_maybe_download_header(_NET_DL_AS_FILE,&h,url,VP(),VP_L());
  struct stat st=empty_stat;
  if (gotHeader){
	st.st_size=h.size;
	st.st_mtime=h.mtime;
  }
  yes_zero_no_t updateSuccess=ZERO;
  zpath_set_realpath_root_writable(zpath);
  log_debug_now("url: %s  gotHeader: %d zpath st_mtime: %ld   ",url,gotHeader,zpath->stat_rp.st_mtime);
  if (gotHeader && st.st_mtime>zpath->stat_rp.st_mtime) updateSuccess=net_maybe_download(_NET_DL_AS_FILE|_NET_DL_EVEN_IF_EXISTS,zpath)?YES:NO;
  char lnk[PATH_MAX+1]; *lnk=0;
  if (zpath->stat_rp.st_ino) net_mk_hardlink(updateSuccess==YES,lnk,url,h.mtime,RP());
  IF1(WITH_PRELOADRAM,html_is_uptodate(d,updateSuccess,zpath,url,&st,   *lnk?lnk+_writable_path_l:zpath->stat_rp.st_ino?VP():NULL));
  return true;
}



static void net_mk_hardlink(const bool overwrite,char *lnk, const char *url, const time_t mtime,const char *rp){
  char  *lnk_fn=stpcpy(stpcpy(lnk,_writable_path),DIR_INTERNET);  *lnk_fn++='/';
  char additional_link[PATH_MAX+1]={0};
  const bool ro=config_internet_hardlink_filename(lnk_fn, additional_link, PATH_MAX-(lnk_fn-lnk),url,mtime);
#define W() warning(WARN_NET|WARN_FLAG_ERRNO,lnk,"failed create hardlink for %s",rp);
  if (*lnk_fn){
	if (overwrite || !cg_file_exists(lnk)){
	  cg_unlink(lnk);
	  if (link(rp,lnk)) W();
	  if (ro) cg_set_readonly(lnk); // cppcheck-suppress knownConditionTrueFalse
	}
	if (*additional_link && !cg_file_exists(strcat(additional_link,lnk_fn-1)) && link(rp,additional_link)) W();
  }
  #undef W
}
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
// https://tiberriver256.github.io/powershell/gui/html/Show-HTML/
// (Get-Content file.html -Raw) -replace '<[^>]+>', ''
