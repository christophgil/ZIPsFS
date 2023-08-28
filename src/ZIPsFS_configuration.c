//////////////////////////////////////////////////////////////////////////////
/// Skip remote roots if filesystem does not respond after certain time    ///
//////////////////////////////////////////////////////////////////////////////

#define ROOT_OBSERVE_EVERY_DECISECONDS 3 // Check availability of remote roots to prevent blocking
#define LOG2_ROOTS 5  // How many file systems are combined
#define DIRECTORY_CACHE_SIZE (1L<<28) // Size of file to cache directory listings
#define DIRECTORY_CACHE_SEGMENTS 4 // Number of files to cache directory listings. If Exceeded, the directory cache is cleared and filled again.
#define MEMCACHE_READ_BYTES_NUM 16*1024*1024 // When storing zip entries in RAM, number of bytes read in one go
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
static int config_zipentry_to_zipfile(const int approach,const char *virtualpath, char *suffix[]){
  //log_debug_now("config_zipentry_to_zipfile  virtualpath=%s\n",virtualpath);
  const int path_l=strlen(virtualpath);
  if (path_l>20){
    const char *e=virtualpath+path_l;
    int d=0;
    *suffix=NULL;
#define A(x,c) ((d=sizeof(#x)-1),(e[-d]==c && !strcmp(e-d,#x)))
#define C(x) A(x,'.')
    if (C(.SSMetaData)||C(.timeseries.data)||C(.wiff.1.~idx2) || C(.wiff2.bak) || C(.Q1Cal) ||  A(_report.txt,'_')){
      switch(approach){
      case 0: *suffix=".wiff.Zip"; return path_l-d;
      case 1: *suffix=".wiff2.Zip"; return path_l-d;
      }
    }else if (C(.rawIdx)){
      switch(approach){
      case 0: *suffix=".Zip";return path_l;
      }
    }else if (C(.wiff) || C(.wiff2) || C(.wiff.scan)){
      switch(approach){
      case 0: *suffix=".wiff.Zip"; return path_l-d;
      case 1: *suffix=".rawIdx.Zip"; return path_l-d;
      case 2: *suffix=".wiff2.Zip"; return path_l-d;
      }
    }else if (C(.wiff.scan)){
      switch(approach){
      case 0: *suffix=".wiff.Zip"; return path_l-d;
      case 1: *suffix=".rawIdx.Zip"; return path_l-d;
      }
    }else if (C(.raw)){
      switch(approach){
      case 0: *suffix=".Zip"; return path_l;
      case 1: *suffix=".wiff2.Zip"; return path_l-d;
      case 2: *suffix="Idx.Zip"; return path_l;
      }
    }
  }
  return 0;
#undef C
#undef A
}

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
  if (!path)return false;
  const int l=strlen(path);
#define I(s) ENDSWITH(path,l,#s)||
  if (I(/analysis.tdf-wal)  I(/analysis.tdf-journal)   I(/.ciopfs)  I(.quant)  I(/autorun.inf)  I(/.xdg-volume-info) I(/.Trash)false) return true;
#undef I
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
static bool config_store_zipentry_in_memcache(long filesize,const char *path,bool is_compressed){
  if (_is_tdf_or_tdf_bin(path)) return true;
  return false;
}

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
  if (ENDSWITH(parent,strlen(parent),".d.Zip")){
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
static bool config_cache_directory(const char *path, bool isRemote, int changed_seconds_ago){
  return isRemote && changed_seconds_ago>60*60;
}
//////////////////////////////////////////////
/// For debugging problems, the following  ///
/// can be set to 0 can                    ///
//////////////////////////////////////////////

#define DIRCACHE_PUT 1
#define DIRCACHE_GET 1

#define DIRCACHE_OPTIMIZE_NAMES 1
#define DIRCACHE_OPTIMIZE_NAMES_RESTORE 1

#define MEMCACHE_PUT 1
#define MEMCACHE_GET 1
