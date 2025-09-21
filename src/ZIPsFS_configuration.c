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
////////////////////////////////////////////////////
#if WITH_ZIPINLINE
#include "ZIPsFS_configuration_zipfile.c"
#endif //WITH_ZIPINLINE

///////////////////////////////////////////////////////////////////////////////////////////////////
/// When a cached struct stat is found the the cache, is it still valid and for how long?       ///
/// Unlimited for remote branches and files that are read-only files.                           ///
///////////////////////////////////////////////////////////////////////////////////////////////////
#if WITH_STAT_CACHE
static uint64_t config_file_attribute_valid_seconds(const int opt, const char *path,const int path_l){
  /* Available Flags:
     (opt&STAT_CACHE_ROOT_IS_WRITABLE)
     (opt&STAT_CACHE_ROOT_IS_REMOTE)
     (opt&STAT_CACHE_FILE_IS_READONLY)
  */
  if ((opt&STAT_CACHE_ROOT_IS_REMOTE) && cg_endsWithZip(path,path_l)  && (strstr(path,"/fularchiv")||strstr(path,"/store/")||strstr(path,"/CHA-CHA-RALSER-RAW"))){
    return (opt&STAT_CACHE_FILE_IS_READONLY)?UINT64_MAX:1;
  }
  return 0;
}
#endif //WITH_STAT_CACHE
/////////////////////////////////////////////
/// A ZIP file is shown as a folder.
/// Convert the ZIP filename to a virtual folder name
/////////////////////////////////////////////


static bool config_zipfilename_to_virtual_dirname(char *dirname,const char *zipfile,const int zipfile_l){
  *dirname=0;
  const char *e=zipfile+zipfile_l;
  if (zipfile_l>4 && _is_zip(e-4)){
    strcpy(dirname,zipfile);
    if(zipfile_l>9 && !strcmp(zipfile+zipfile_l-6,".d.Zip")){
      dirname[zipfile_l-4]=0;
    }else{
      strcat(dirname,EXT_CONTENT);
    }
    return false; /* false: Not show zipfile itself  true: Also show zipfile*/
  }
  return false;
}
#if WITH_ZIPINLINE
static bool config_skip_zipfile_show_zipentries_instead(const char *zipfile,const int zipfile_l){
  if(zipfile_l>20 && _is_Zip(zipfile+zipfile_l-4)){
    static int WITHOUT_PARENT_LEN[10];
    static const char *WITHOUT_PARENT[]={".wiff.Zip",".wiff2.Zip",".rawIdx.Zip",".raw.Zip",NULL};
    if (!WITHOUT_PARENT_LEN[0]){
      for(int i=0;WITHOUT_PARENT[i];i++) WITHOUT_PARENT_LEN[i]=strlen(WITHOUT_PARENT[i]);
    }
    for(int i=0;WITHOUT_PARENT[i];i++){
      if (!strcmp(zipfile+zipfile_l-WITHOUT_PARENT_LEN[i],WITHOUT_PARENT[i])) return true;
    }
  }
  return false;
}
#endif //WITH_ZIPINLINE

/////////////////////////////////////////////////////////////////////////////////
/// With the bruker dll, two non existing files are requested extremely often. //
/// Improving performance by refusing those file names                         //
/////////////////////////////////////////////////////////////////////////////////
static bool config_not_report_stat_error(const char *path,const int path_l){
#define I(s) ENDSWITH(path,path_l,#s)||
#define S(s,pfx) !strncmp(path,pfx,sizeof(pfx)-1)||
  return I(/analysis.tdf-wal)  I(/analysis.tdf-journal)   I(/.ciopfs)  I(.quant)  I(/autorun.inf)  I(/.xdg-volume-info)  S(path,"/.Trash")  S(path,"/ZIPsFS_") false;
#undef I
#undef S
}
#if WITH_MEMCACHE


////////////////////////////////////////////////////////////
/// Rules which files or zip entries are cached into RAM ///
/// This is applied for command-line-option   -c rule    ///
/// Return value:                                        ///
///   < 0: No cache                                      ///
///   > 0: Cache is advised and requires n bytes         ///
////////////////////////////////////////////////////////////
static off_t config_advise_cache_in_ram(const int flags,const char *virtualpath, const int vp_l, const char *realpath,const int rp_l,const char *rootpath,const off_t filesize){
  //if (!(flags&ADVISE_CACHE_IS_ZIPENTRY)) return -1;
  const char *e=virtualpath+vp_l;
  if (vp_l>4 && e[-4]=='.' && (e[-3]|32)=='e' && (e[-2]|32)=='x' && (e[-1]|32)=='e') return -1; /* The exe files hold the icon for File Explorer */
  if (flags&ADVISE_CACHE_BY_POLICY) return filesize;
  off_t need=filesize;
  bool cache=((flags&ADVISE_CACHE_IS_CMPRESSED)&&(flags&ADVISE_CACHE_IS_SEEK_BW)) || ENDSWITH(virtualpath,vp_l,"analysis.tdf_bin");
  //if (DEBUG_NOW==DEBUG_NOW && !cache) {cache=ENDSWITH(virtualpath,vp_l,".mzML"); }
  if (!cache && ENDSWITH(virtualpath,vp_l,"analysis.tdf") && vp_l+4<MAX_PATHLEN){ /* Note: timsdata.dll opens analysis.tdf first  and then analysis.tdf_bin */
    cache=true;
    char tdf_bin[MAX_PATHLEN+1]; stpcpy(stpcpy(tdf_bin,virtualpath),"_bin");
    struct stat st_tdf_bin;
    if (!statForVirtualpathAndRootpath(&st_tdf_bin,tdf_bin,rootpath)){
      log_warn("%s:%d Warning: Missing file %s\n",__func__,__LINE__,tdf_bin);
      return false;
    }
    need+=st_tdf_bin.st_size;
  }
  return cache?need:-1;
}
#endif //WITH_MEMCACHE


static bool config_advise_evict_from_filecache(const char *realpath,const int realpath_l, const char *zipentryOrNull, const off_t filesize){
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
static bool config_advise_cache_directory_listing(const char *path,const int path_l,const struct timespec mtime, const int flags){
  if (flags&ADVISE_DIRCACHE_IS_ZIP) return true;
  if (flags&ADVISE_DIRCACHE_IS_REMOTE){
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
static long config_search_file_which_roots(const char *virtualpath,const int virtualpath_l){
  return 0xFFFFffffFFFFffffL;
}
static bool config_readir_no_other_roots(const char *realpath,const int realpath_l){
#define C(sfx) ENDSWITH(realpath,realpath_l,#sfx)
  return C(.wiff2.Zip) || C(.wiff.Zip) || C(.rawIdx.Zip) || C(.raw.Zip) || C(.d.Zip);
#undef C
}
////////////////////////////
/// Retry on failure.    ///
////////////////////////////
static long config_num_retries_getattr(const char *path, const int path_l, int *sleep_milliseconds){
  return 1; // FIXME
  if (_is_tdf_or_tdf_bin(path)){
    //DIE_DEBUG_NOW("%s",path);
    *sleep_milliseconds=1000;
    return 3;
  }
  return 1;
}


//////////////////////////////////////////////
/// File attributes                        ///
// analysis.tdf is an SQLITE file.         ///
/// The hope is that SQLITE will be faster ///
//////////////////////////////////////////////
static bool config_file_is_readonly(const char *path, const int path_l){
  return ENDSWITH(path,path_l,"analysis.tdf");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Given the absolute path of the ZIP file or folder and the list of                                                               ///
/// contained files, some may be excluded by setting the respective file name to NULL                                               ///
/// For Zip files with brukertimstof files, retaining only analysis.tdf and analysis.tdf_bin saves 15 seconds per record            ///
/// For brukertimstof dot-d folders  the files analysis.tdf-wal and analysis.tdf-shm may cause page faults / segfaults. Hide them   ///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void config_exclude_files(const char *path, const int path_l, const int num_files, char **files, const off_t *fsize){
  /* If it is a Zip file ending with .d.Zip which has members analysis.tdf and analysis.tdf_bin, then keep only those two */
  const bool is_dotd=path_l>10  && ENDSWITH(path,path_l,".d") && !strncmp(path,_root_writable->rootpath,_root_writable->rootpath_l);
  const bool is_dotd_Zip=(path_l>10  && ENDSWITH(path,path_l,".d.Zip"));
  if (is_dotd_Zip || is_dotd){
    //  if (path_l>10  && !strcmp(".d.Zip",path+path_l-6)){
    for(int i=path_l; --i>=0;){
      if (path[i]=='/'){
        if (path[i+1]=='2'){
          if (is_dotd){
            for(int j=num_files; --j>=0;){
              if (files[j] && (!strcmp("analysis.tdf-wal",files[j])||!strcmp("analysis.tdf-shm",files[j]))){
                files[j]=NULL;
              }
            }
          }
          if (is_dotd_Zip){
            int count=0;
            for(int k=2; --k>=0;){ /* First round: look for analysis.tdf and .tdf_bin.   Second: set to NULL. */
              for(int j=num_files; --j>=0;){
                if (files[j] && *files[j]=='a' && (!strcmp("analysis.tdf",files[j])||!strcmp("analysis.tdf_bin",files[j]))){
                  if (k) count++;
                }else if (count>1 && !k){
                  files[j]=NULL;
                }
              }
            }
          }
        }
        break;
      } /* '/' */
    }
  }
}



////////////////////////////////////////////////////////////////////////////////////
/// Files are generally stored in the first root whenever                        ///
/// files are copied into the virtual file systems or                            ///
/// when files are autmotically generated like tsv from parquet                  ///
///                                                                              ///
/// If false is returned, then the error message will be                         ///
///     No space left on device                                                  ///
////////////////////////////////////////////////////////////////////////////////////
static bool config_has_sufficient_storage_space(const char *realpath, const long availableBytes, const long totalBytes){
  return availableBytes<totalBytes*0.75;
}


//////////////////////////////////////////////////////////////////////
/// Data files downloaded from the internet                        ///
//////////////////////////////////////////////////////////////////////

#if WITH_INTERNET_DOWNLOAD
static bool config_internet_update_header(const char *url, const struct stat *st){
  return time(NULL)-st->st_mtime>60*60;
}

static bool configuration_internet_with_date_in_filename(const char *url){
  return true;
}
#endif //WITH_INTERNET_DOWNLOAD
