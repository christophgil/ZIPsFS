/////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                          ///
/// This file is for customizations by the user  ///
/// The functions start with prefix  "config_"   ///
////////////////////////////////////////////////////


///////////////////////////////
/// Local helper functions  ///
///////////////////////////////

static bool _is_zip(const char *s){
  return *s=='.' && (s[1]|32)=='z' && (s[2]|32)=='i' && (s[3]|32)=='p';
}
static bool _is_Zip(const char *s){
  return *s=='.' && s[1]=='Z' && s[2]=='i' && s[3]=='p';
}
static bool _is_tdf_or_tdf_bin(const char *path){
  if (!path || !*path) return false;
  const int slash=cg_last_slash(path);
  return !strcmp(path+slash+1,"analysis.tdf") || !strcmp(path+slash+1,"analysis.tdf_bin");
}
////////////////////////////////////////////////////////////////////////////////////////////////
/// Forming zip file path from virtual path by removing terminal characters and appending a string.
/// Parameters: b and e  - beginning and ending of the virtual file name.
/// return if not match: INT_MAX
/// return on success: Amount of bytes to be deleted from the end e of the path.
///               append: String that needs to be added.
/// Here:   Virtual folders ending with
///            - .zip.Content originate
///            - .d
///
//////////////////////////////////////////////////////////////
static const int config_virtual_dirpath_to_zipfile(const char *b, const char *e,char *append[]){
  if (e-b>12 && _is_zip(e-12)  && !memcmp(e-8,EXT_CONTENT,8)) return -8;
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
/// See: config_skip_zipfile_show_zipentries_instead()
///      config_containing_zipfile_of_virtual_file_test()
////////////////////////////////////////////////////
#if WITH_ZIPINLINE
static int config_containing_zipfile_of_virtual_file(const int approach,const char *path,const int path_l,char *suffix[]){
#define A(x,c) ((d=sizeof(#x)-1),(e[-d]==c && !strcmp(e-d,#x)))
#define C(x) A(x,'.')
#define S(x) if (suffix)*suffix=x;return path_l-d
  if (path_l>20){
    const char *e=path+path_l;
    int d=0;
    if (suffix) *suffix=NULL;
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
static void config_containing_zipfile_of_virtual_file_test(){
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
    const int f_l=strlen(*f);
    fprintf(stderr,"\n\x1B[7m%s\x1B[0m\n",*f);
    char *suffix;
    for(int len,i=0;(len=config_containing_zipfile_of_virtual_file(i,*f,f_l, &suffix))!=0;i++){
      strcpy(str,*f);
      strcpy(str+len,suffix);
      fprintf(stderr,"%d  approach=%d %s\n",i,len,str);
    }
  }
  fprintf(stderr,"Going to EXIT from "__FILE__":%d\n",__LINE__);
  EXIT(0);
}
#endif //WITH_ZIPINLINE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// keep_cache:1;  Can be filled in by open.
/// It signals the kernel that any currently cached file data (ie., data that the filesystem provided the last time the file was open) need not be invalidated.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if WITH_STAT_CACHE
static uint32_t config_file_attribute_valid_seconds(const bool isReadOnly, const char *path,const int path_l){
  if (isReadOnly && cg_endsWithZip(path,path_l)  && (strstr(path,"/fularchiv01")||strstr(path,"/CHA-CHA-RALSER-RAW"))) return UINT32_MAX;
  return 0;
}
#endif //WITH_STAT_CACHE
/////////////////////////////////////////////
/// A ZIP file is shown as a folder.
/// Convert the ZIP filename to a virtual folder name
/////////////////////////////////////////////
static char *config_zipfilename_to_virtual_dirname(char *dirname,const char *zipfile,const int zipfile_l){
  strcpy(dirname,zipfile);
  const char *e=zipfile+zipfile_l;
  if (zipfile_l>4 && _is_zip(e-4)){
    if(zipfile_l>9 && !strcmp(zipfile+zipfile_l-6,".d.Zip")){
      dirname[zipfile_l-4]=0;
    }else{
      strcat(dirname,EXT_CONTENT);
    }
  }
  return dirname;
}
#if WITH_ZIPINLINE
static bool config_skip_zipfile_show_zipentries_instead(const char *zipfile,const int zipfile_l){
  static const char *WITHOUT_PARENT[]={".wiff.Zip",".wiff2.Zip",".rawIdx.Zip",".raw.Zip",NULL};
  static int WITHOUT_PARENT_LEN[10];
  if(zipfile_l>20 && _is_Zip(zipfile+zipfile_l-4)){
    if (!WITHOUT_PARENT_LEN[0]){
      for(int i=0;WITHOUT_PARENT[i];i++) WITHOUT_PARENT_LEN[i]=strlen(WITHOUT_PARENT[i]);
    }
    for(int i=0;WITHOUT_PARENT[i];i++){
      if (!strcmp(zipfile+zipfile_l-WITHOUT_PARENT_LEN[i],WITHOUT_PARENT[i])) return true;
    }
  }
  return false;
}
static bool config_also_show_zipfile_in_listing(const char *zipfile,const int zipfile_l){ /* Not only the content of the zip file but also the zip file itself. */
  return zipfile_l>5 && _is_zip(zipfile+zipfile_l-4) && !_is_Zip(zipfile+zipfile_l-4);
}
#endif //WITH_ZIPINLINE
/////////////////////////////////////////////////////////////////////////////////
/// With the bruker dll, two non existing files are requested extremely often. //
/// Improving performance by refusing those file names                         //
/////////////////////////////////////////////////////////////////////////////////
static bool config_not_report_stat_error(const char *path,const int path_l){
#define I(s) ENDSWITH(path,path_l,#s)||
  return I(/analysis.tdf-wal)  I(/analysis.tdf-journal)   I(/.ciopfs)  I(.quant)  I(/autorun.inf)  I(/.xdg-volume-info) I(/.Trash) !strncmp(path,"/ZIPsFS_",8);
#undef I
}
////////////////////////////////////////////////////
/// Rules which zip entries are cached into RAM  ///
/// This is active when started with option      ///
///      -c rule                                 ///
////////////////////////////////////////////////////
#if WITH_MEMCACHE
static bool config_advise_cache_zipentry_in_ram(long filesize,const char *path,const int path_l,bool is_compressed){
  return _is_tdf_or_tdf_bin(path);
}
#endif //WITH_MEMCACHE
static bool config_advise_evict_from_filecache(const char *realpath,const int realpath_l, const char *zipentryOrNull, const int zipentry_l){
  return _is_tdf_or_tdf_bin(zipentryOrNull) || _is_tdf_or_tdf_bin(realpath);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Lots of stupid redundand FS requests while tdf and tdf_bin files are read.                              ///
/// We create a temporary cache for storing file attributes and associate this cache to the file descriptor ///
/// When the file is closed, the cache is disposed.                                                         ///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if WITH_TRANSIENT_ZIPENTRY_CACHES
static bool config_advise_transient_cache_for_zipentries(const char *path, const int path_l){
  return _is_tdf_or_tdf_bin(path);
}
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
/////////////////////////////////////////////////////////////////////////////////////////
/// Windows will read the beginning of exe files to get the icon                      ///
/// With the rule  -c always, all .exe files would be loaded into RAM                 ///
/// when a file listing is displayed in File Explorerer.                              ///
/// Add here exceptions in case of MEMCACHE_ALWAYS selected with CLI Option -c ALWAYS ///
/////////////////////////////////////////////////////////////////////////////////////////
static bool config_advise_not_cache_zipentry_in_ram(const char *path, const int path_l){
  const char *e=path+path_l;
  return path_l>4 && e[-4]=='.' && (e[-3]|32)=='e' && (e[-2]|32)=='x' && (e[-1]|32)=='e';
}

////////////////////////////////////////////////////////////
/// Certain files may be hidden in file listings         ///
////////////////////////////////////////////////////////////
static bool config_do_not_list_file(const char *parent, const char *filename,const int filename_l){
#define C(x) ENDSWITH(filename,filename_l,#x)
  return filename_l>12 && C(.md5) && (C(.Zip.md5) || C(.wiff.md5) || C(.wiff.scan.md5) || C(.raw.md5) || C(.rawIdx.md5));
#undef C
}
////////////////////////////////////////////////////////////
/// Normally, one could replace a file by a new version  ///
/// The new file is stored in the first root.            ///
/// Certain files may be protected from beeing changed.  ///
////////////////////////////////////////////////////////////
static bool config_not_overwrite(const char *path,const int path_l){
#define C(a) ENDSWITH(path,path_l,#a)
  return C(.tdf)||C(.tdf_bin)||C(.wiff)||C(.wiff2)||C(.wiff.scan)||C(.raw)||C(.rawIdx);
#undef C
}

////////////////////////////////////////////////////////////
/// What about file directories, should they be cached?  ///
/// Maybe remote folders that have not changed recently  ///
////////////////////////////////////////////////////////////
#if WITH_DIRCACHE
static bool config_advise_cache_directory_listing(const char *path,const int path_l,const bool isRemote,const bool isZipArchive, const struct timespec mtime){
  if (isZipArchive) return true;
  if (isRemote){
    struct timeval tv={0};
    gettimeofday(&tv,NULL);
    return (tv.tv_sec-mtime.tv_sec)>60*60;
  }
  return false;
}
#endif //WITH_DIRCACHE

////////////////////////////////////////////////////////////////////
/// Register frequent file extensions to improve performance.    ///
/// They do not need to start with dot.                          ///
////////////////////////////////////////////////////////////////////
static const int _FILE_EXT_FROM=__COUNTER__;
static char *config_some_file_path_extensions(const char *path, int path_l, int *return_index){
#define C(a) if (ENDSWITH(path,path_l,#a)){ *return_index=__COUNTER__-_FILE_EXT_FROM; return #a;}
  switch(path[path_l-1]){
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
////////////////////////////////////////////////////////////////////
/// Restrict search for certain files to improve performance.    ///
////////////////////////////////////////////////////////////////////
static long config_search_file_which_roots(const char *virtualpath,const int virtualpath_l,const bool path_starts_with_autogen){
#if WITH_AUTOGEN
  if (path_starts_with_autogen && _config_ends_like_a_generated_file(virtualpath,virtualpath_l)) return 1;
#endif //WITH_AUTOGEN
  return 0xFFFFffffFFFFffffL;
}
static long config_num_retries_getattr(const char *path, const int path_l,int *sleep_milliseconds){
  if (_is_tdf_or_tdf_bin(path)){
    *sleep_milliseconds=1000;
    return 3;
  }
  return 1;
}
