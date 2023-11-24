#define FILE_CTRL_CLEARCACHE "/ZIPsFS_.CLEbAR=CACHE+fas-dfasdgfgaikuwty"

static const char *special_file_content(const enum enum_special_files i){
  switch(i){
  case SFILE_README: return "<HTML><BODY>\nThis is the virtual file system ZIPsFS<BR>\n\
It can expand ZIP files and combine two or more file trees.<BR>\n\
Records consisting of several files archived as ZIP file can be presented as single files as if they would not have been zipped.<BR>\n\
It pretends write permission for read-only file locations.<BR>\n\
<A href=\"" HOMEPAGE "\">Homepage</A><BR>\n\
</BODY></HTML>\n";
  case SFILE_CTRL: return
#include "ZIPsFS_ctrl.bash.inc"
      ;
  default:   return NULL;
  }
}

static void trigger_files(const char *path){
  debug_trigger_files(path);
  if (!strcmp(path,FILE_CTRL_CLEARCACHE)){
    warning(WARN_MISC,FILE_CTRL_CLEARCACHE,"");
    dircache_clear_if_reached_limit_all();
  }
}
static int read_special_file(const char *path, char *buf, const size_t size, const off_t offset){
  const int i=whatSpecialFile(path);
  if (i<0) return 0;
  const char *content=special_file_content(i);
  //log_debug_now("i=%d content=%s\n",i,content);
  int l=my_strlen(content);
  LOCK(mutex_special_file,
       if (i==SFILE_FS_INFO) do{
           _size_info*=2;
           FREE2(_info);
           content=_info=malloc(_size_info+17);
           l=print_all_info();
         }while(l>_size_info-256);

       const int n=max_int(0,min_int(l-offset,min_int(size,l-(int)offset))); // num bytes to be copied
       if (n<=0)  return EOF;
       if (n>0) memcpy(buf,content+offset,n);
       FREE2(_info);
       );
  return n<=0?EOF:n;
}
