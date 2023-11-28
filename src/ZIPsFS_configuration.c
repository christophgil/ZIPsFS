////////////////////////
/// Helper functions ///
////////////////////////
static bool is_zip(const char *s){
  return *s=='.' && (s[1]|32)=='z' && (s[2]|32)=='i' && (s[3]|32)=='p';
}
static bool is_Zip(const char *s){
  return *s=='.' && s[1]=='Z' && s[2]=='i' && s[3]=='p';
}

////////////////////////////////////////////////////////////////////////////////////////////////
/// Forming zip file path from virtual path by removing terminal characters and adding a string.
/// Parameters: b and e  - beginning and ending of the virtual file name.
/// return if not match: INT_MAX
/// return on success: Amount of bytes to be deleted from the end e of the path.
///               append: String that needs to be added.
//////////////////////////////////////////////////////////////
static const int config_virtualpath_is_zipfile(const char *b, const char *e,char *append[]){
  if (e-b>12 && is_zip(e-12)  && !memcmp(e-8,".Content",8)) return -8;
  if (e[-2]=='.' && (e[-1]|32)=='d') {
    *append=".Zip";
    return 0;
  }
  return INT_MAX;
}


////////////////////////////////////////////////////
/// Files contained in zip files may be displayed without parent dir.
/// Assuming a virtual path    subdir/20230320_blablabla.rawIdx
/// originating from the zip file subdir/20230320_blablabla.rawIdx.Zip
/// having a zip file entry   20230320_blablabla.rawIdx
///
/// More than one possibility need to be considered - hence switch(approach) ... statements.
/// For example xxxxx.wiff may come from a zip file xxxxx.wiff.Zip or xxxxx.rawIdx.Zip.
///
/// return:
///   no match: 0
///   On match return the length of the substring that together with suffix forms the zip file.
///
/// See: config_zipentries_instead_of_zipfile()
///      config_zipentry_to_zipfile_test()
////////////////////////////////////////////////////
static int config_zipentry_to_zipfile(const int approach,const char *path, char *suffix[]){
  //log_debug_now("config_zipentry_to_zipfile  path=%s\n",path);
  const int path_l=strlen(path);
  if (path_l>20){
    const char *e=path+path_l;
    int d=0;
    if (suffix) *suffix=NULL;
#define A(x,c) ((d=sizeof(#x)-1),(e[-d]==c && !strcmp(e-d,#x)))
#define C(x) A(x,'.')
#define S(x) if (suffix)*suffix=x;return path_l-d
    if (C(.wiff) || C(.wiff2) || C(.wiff.scan)){
      switch(approach){
      case 0: S(".wiff.Zip");
      case 1: S(".rawIdx.Zip");
      case 2: S(".wiff2.Zip");
      }
    }else if (C(.rawIdx)){
      switch(approach){
      case 0: S(".rawIdx.Zip");
      }
    }else if (C(.raw)){
      switch(approach){
      case 0: S(".raw.Zip");
      case 1: S(".wiff2.Zip");
      case 2: S(".rawIdx.Zip");
      }
    }else if (C(.SSMetaData) || C(.timeseries.data) || A(_report.txt,'_')){
      switch(approach){
      case 0: S(".wiff.Zip");
      case 1: S(".wiff2.Zip");
      }
    }
  }
  return 0;
#undef S
#undef C
#undef A
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// keep_cache:1;  Can be filled in by open.
/// It signals the kernel that any currently cached file data (ie., data that the filesystem provided the last time the file was open) need not be invalidated.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if IS_STAT_CACHE
static uint32_t config_file_attribute_valid_seconds(const bool isReadOnly, const char *path,const int path_l){
  if (isReadOnly && endsWithZip(path,path_l)  && (strstr(path,"/fularchiv01")||strstr(path,"/CHA-CHA-RALSER-RAW"))) return UINT32_MAX;
  return 0;
}
#endif //IS_STAT_CACHE
//S_ISREG(st->st_mode) && !(st->st_mode&( S_IWUSR| S_IWGRP |S_IWOTH))
static void config_zipentry_to_zipfile_test(){
  if (true) return; /* Remove to run tests */
  char *ff[]={
    "20230310_aaaaaaaaaaa.wiff",
    "20230310_aaaaaaaaaaa.wiff.scan",
    "20230310_aaaaaaaaaaa.wiff2",
    "20230310_aaaaaaaaaaa.rawIdx",
    "20230310_aaaaaaaaaaa.raw",
    NULL};
  char str[99];
  for(char **f=ff;*f;f++){
    printf("\n\x1B[7m%s\x1B[0m\n",*f);
    char *suffix;
    for(int len,i=0;(len=config_zipentry_to_zipfile(i,*f, &suffix))!=0;i++){
      strcpy(str,*f);
      strcpy(str+len,suffix);
      printf("%d  approach=%d %s\n",i,len,str);
    }
  }
  exit(0);
}



/////////////////////////////////////////////
/// Convert a real path to a virtual path ///
/////////////////////////////////////////////
static char *config_zipfilename_real_to_virtual(char *new_name,const char *n){
  const int len=strlen(n);
  strcpy(new_name,n);
  const char *e=n+len;
  if (len>4 && is_zip(e-4)){
    if(len>9 && !strcmp(n+len-6,".d.Zip")){
      new_name[len-4]=0;
    }else{
      strcat(new_name,".Content");
    }
  }
  return new_name;
}
static const char *WITHOUT_PARENT[]={".wiff.Zip",".wiff2.Zip",".rawIdx.Zip",".raw.Zip",NULL};
static int WITHOUT_PARENT_LEN[10];
static bool config_zipentries_instead_of_zipfile(const char *realpath){
  const int len=strlen(realpath);
  const char *e=realpath+len;
  if(len>20 && is_Zip(e-4)){
    for(int i=0;WITHOUT_PARENT[i];i++){
      if (!WITHOUT_PARENT_LEN[i])  WITHOUT_PARENT_LEN[i]=strlen(WITHOUT_PARENT[i]);
      if (!strcmp(e-WITHOUT_PARENT_LEN[i],WITHOUT_PARENT[i])) return true;
    }
  }
  return false;
}
static bool config_also_show_zipfile_in_listing(const char *path){ /* Not only the content of the zip file but also the zip file itself. */
  const int len=strlen(path);
  return len>5 && is_zip(path+len -4) && !is_Zip(path+len-4);
}
/////////////////////////////////////////////////////////////////////////////////
/// With the bruker dll, two non existing files are requested extremely often. //
/// Improving performance by refusing those file names                         //
/////////////////////////////////////////////////////////////////////////////////
static bool config_not_report_stat_error(const char *path ){
  if (path){
  const int l=strlen(path);
#define I(s) ENDSWITH(path,l,#s)||
  if (I(/analysis.tdf-wal)  I(/analysis.tdf-journal)   I(/.ciopfs)  I(.quant)  I(/autorun.inf)  I(/.xdg-volume-info) I(/.Trash)
      !strncmp(path,"/ZIPsFS_",8)
      ) return true;
#undef I
  }
  return false;
}
static bool _is_tdf_or_tdf_bin(const char *path){
  if (!path || !*path)return false;
  const int slash=last_slash(path);
  return !strcmp(path+slash+1,"analysis.tdf") || !strcmp(path+slash+1,"analysis.tdf_bin");
}
////////////////////////////////////////////////////
/// Rules which zip entries are cached into RAM  ///
/// This is active when started with option      ///
///      -c rule                                 ///
////////////////////////////////////////////////////
#if IS_MEMCACHE
static bool config_store_zipentry_in_memcache(long filesize,const char *path,bool is_compressed){
  if (_is_tdf_or_tdf_bin(path)) return true;
  return false;
}
#endif //IS_MEMCACHE
static bool configuration_evict_from_filecache(const char *realpath,const char *zipentryOrNull){
  if (_is_tdf_or_tdf_bin(zipentryOrNull) ||
      _is_tdf_or_tdf_bin(realpath)) return true;
  return false;
}
/////////////////////////////////////////////////////////////////////////
/// Windows will read the beginning of exe files to get the icon      ///
/// With the rule  -c always, all .exe files would be loaded into RAM ///
/// when a file listing is displayed in File Explorerer               ///
/////////////////////////////////////////////////////////////////////////
static bool config_not_memcache_zip_entry(const char *path){
  const int len=!path?0:strlen(path);
  const char *e=path+len;
  return len>4 && e[-4]=='.' && (e[-3]|32)=='e' && (e[-2]|32)=='x' && (e[-1]|32)=='e';  /* if endsWithIgnoreCase .exe  */
}
//////////////////////////////////////////////////////////////
/// This activates  fuse_file_info->keep_cache  upon open ////
//////////////////////////////////////////////////////////////
static bool config_keep_file_attribute_in_cache(const char *path){
  if (!_is_tdf_or_tdf_bin(path)) return false;
  const int slash=last_slash(path);
  return slash>0 && !strncmp(path+slash,"/202",4);
}

/////////////////////////////////////////////////////
/// For a certain software we use it is important ///
/// only certain files in a ZIP are visible       ///
////////////////////////////////////////////////////
static bool config_zipentry_filter(const char *parent, const char *name){
  return true;
  if (parent && ENDSWITH(parent,strlen(parent),".d.Zip")){
    const int slash=last_slash(parent);
    if (slash>0 && !strncmp(parent+slash,"/202",4) && (strstr(parent,"_PRO1_")||strstr(parent,"_PRO1_")||strstr(parent,"_PRO1_"))){
      return _is_tdf_or_tdf_bin(name);
    }
  }
  return true;
}
////////////////////////////////////////////////////////////
/// Certain files may be hidden in file listings         ///
////////////////////////////////////////////////////////////
static bool config_filter_files_in_listing(const char *parent, const char *name){
  const int l=my_strlen(name);
#define C(x) ENDSWITH(name,l,#x)
  if (l>12 && C(.md5) && (C(.Zip.md5) || C(.wiff.md5) || C(.wiff.scan.md5) || C(.raw.md5) || C(.rawIdx.md5))) return false;
#undef C
  return true;
}
////////////////////////////////////////////////////////////
/// Normally, one could replace a file by a new version  ///
/// The new file is stored in the first root.            ///
/// Certain files may be protected from beeing changed.  ///
////////////////////////////////////////////////////////////
static bool config_not_overwrite(const char *path){
  const int l=strlen(path);
#define C(a) ENDSWITH(path,l,#a)
  return C(.tdf)||C(.tdf_bin)||C(.wiff)||C(.wiff2)||C(.wiff.scan)||C(.raw)||C(.rawIdx);
#undef C
}

////////////////////////////////////////////////////////////
/// What about file directories, should they be cached?  ///
/// Maybe remote folders that have not changed recently  ///
////////////////////////////////////////////////////////////
#if IS_DIRCACHE
static bool config_cache_directory(const char *path, bool isRemote, const struct timespec mtime){
  if (isRemote){
    struct timeval tv={0};
    gettimeofday(&tv,NULL);
    return (tv.tv_sec-mtime.tv_sec)<60*60;
  }
  return false;
}
#endif //IS_DIRCACHE

////////////////////////////////////////////////////////////////////
/// Register frequent file extensions to improve performance.    ///
/// They do not need to start with dot.                          ///
////////////////////////////////////////////////////////////////////
const int _FILE_EXT_FROM=__COUNTER__;
static char *some_file_path_extensions(const char *path, int len, int *return_index){
  assert(path[len]==0);
#define C(a) if (ENDSWITH(path,len,#a)){ *return_index=__COUNTER__-_FILE_EXT_FROM; return #a;}
  switch(path[len-1]){
  case 'a': C(.timeseries.data);break;
  case 'd': C(.d);break;
  case 'f': C(.wiff);C(.tdf);C(desktop.inf);C(Desktop.inf);break;
  case 'l': C(.tdf-journal); C(.tdf-wal);break;
  case 'n': C(.wiff.scan);C(.tdf_bin);break;
  case 't': C(.txt);break;
  case 'w': C(.raw);break;
  case 'x': C(.rawIdx);break;
  }
#undef C
  return NULL;
}
const int _FILE_EXT_TO=__COUNTER__;

////////////////////////////////////////////////////////////////////////
/// Bruker software tests existence of certain files thousands of times.
////////////////////////////////////////////////////////////////////////

#if DO_REMEMBER_NOT_EXISTS
static bool do_remember_not_exists(const char *path){
  const int len=strlen(path);
  if (!len) return false;
#define C(x) ENDSWITH(path,len,#x)
  if (C(.tdf-journal) || C(.tdf-wal) || C(/.ciopfs)){
#undef C
    const char *pro=strstr(path,"/PRO");
    return (pro && '0'<=pro[4] && pro[4]<='9' && pro[5]=='/');
  }
  return false;
}
#endif //DO_REMEMBER_NOT_EXISTS
