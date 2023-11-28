#define FILE_CTRL_CLEARCACHE "/ZIPsFS_.CLEbAR=CACHE+fas-dfasdgfgaikuwty"

#define OPT_STR_REPLACE_DRYRUN (1<<0)
#define OPT_STR_REPLACE_ASSERT (1<<1)

static int str_replace(const int opt,  char *haystack,  int h_l, const char *needle,  int n_l, const char *replacement,  int r_l){
  assert(haystack!=NULL);assert(needle!=NULL);assert(replacement!=NULL);
  if (!h_l) h_l=strlen(haystack);
  if (!r_l) r_l=strlen(replacement);
  if (!n_l) n_l=strlen(needle);
  //fprintf(stderr,"lllllllllllllllllll h_l=%d  n_l=%d   r_l=%d  \n",h_l,n_l,r_l);
  bool replaced=false;
  for(int h=h_l-n_l+1;--h>=0;){
    if (haystack[h]!=needle[0] || memcmp(haystack+h,needle,n_l)) continue;
    replaced=true;
    const int diff=r_l-n_l;
    h_l+=diff;
    if (!(opt&OPT_STR_REPLACE_DRYRUN)){
      if (diff<0){ /* Shift left, gets-smaller */
        for(int p=h+r_l;p<h_l;p++) haystack[p]=haystack[p-diff];
      }else if (diff>0){ /* Shift right */
        for(int p=h_l; --p>=h+r_l;) haystack[p]=haystack[p-diff];
      }
    }
    memcpy(haystack+h,replacement,r_l);
  }
  if (!(opt&OPT_STR_REPLACE_DRYRUN)) haystack[h_l]=0;
  if (opt&OPT_STR_REPLACE_ASSERT) assert(replaced);
  return h_l;
}

static char *shell_script(const enum enum_special_files i){
#include "ZIPsFS_ctrl.bash.inc"
#define C(f) str_replace(OPT_STR_REPLACE_ASSERT,texts[i],0,#f,0,f,0)
  static char *texts[SFILE_L]={0};
  static long mtime=0;
  static char *begin=NULL;
  if (!mtime){
    assert(0!=(mtime=atol(orig)));
    begin=strchr(orig,'\n')+1;
  }
  if (!texts[i]){
    texts[i]=malloc(strlen(begin)+1000);
    strcpy(texts[i],begin);
    C(FILE_CTRL_CLEARCACHE);
    assert(i==SFILE_DEBUG_CTRL || i==SFILE_CTRL);
    if(i==SFILE_DEBUG_CTRL){
      C(FILE_DEBUG_CANCEL);
      C(FILE_DEBUG_BLOCK);
      C(FILE_DEBUG_KILL);
      str_replace(OPT_STR_REPLACE_ASSERT,texts[i],0,"FOR_ALL=1",0,"FOR_ALL=0",0);
      struct stat stbuf;
      stat(_debug_ctrl,&stbuf);
      if (stbuf.st_mtime<=mtime){
        unlink(_debug_ctrl);
        FILE *fo=fopen(_debug_ctrl,"w");
        if (fo){
          fputs(texts[i],fo);
          fflush(fo);
          fclose(fo);
          log_msg("Written %s\n", _debug_ctrl);
          syscall(SYS_chmod,_debug_ctrl,0755);
        }else{
          warning(WARN_MISC,_debug_ctrl,"Failed fopen(...,w)");
        }
      }
    }
  }
  return texts[i];
#undef C
}

static const char *special_file_content(const enum enum_special_files i){
  switch(i){
  case SFILE_README: return "<HTML><BODY>\nThis is the virtual file system ZIPsFS<BR>\n\
It can expand ZIP files and combine two or more file trees.<BR>\n\
Records consisting of several files archived as ZIP file can be presented as single files as if they would not have been zipped.<BR>\n\
It pretends write permission for read-only file locations.<BR>\n\
<A href=\"" HOMEPAGE "\">Homepage</A><BR>\n\
</BODY></HTML>\n";
  case SFILE_CTRL: return shell_script(i);
  default: return NULL;
  }
}
static void trigger_files(const char *path){
  debug_trigger_files(path);
  if (!strcmp(path,FILE_CTRL_CLEARCACHE)){
    warning(WARN_MISC,FILE_CTRL_CLEARCACHE,"");
    dircache_clear_if_reached_limit_all();
  }
}
static int read_special_file(const int i, char *buf, const size_t size, const off_t offset){
  const char *content;
  int l;
  LOCK(mutex_special_file,
       if (i==SFILE_INFO){
         content=_info;
         l=_info_l;
       }else{
         l=my_strlen(content=special_file_content(i));
       }
       const int n=min_int(size,l-(int)offset); // num bytes to be copied
       if (n>0) memcpy(buf,content+offset,n));
  return n<0?EOF:n;
}
static void make_info(){
  assert_locked(mutex_special_file);
  int l;
  do{
    _info_capacity*=2;
    FREE2(_info);
    _info=malloc(_info_capacity+17);
    l=_info_l=print_all_info();
  }while(l>_info_capacity-256);

}
