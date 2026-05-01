/************************************************************************/
/* COMPILE_MAIN=ZIPsFS                                                  */
/* Special generated files                                              */
/************************************************************************/
_Static_assert(WITH_PRELOADRAM,"");
#define C(txt) textbuffer_add_segment(TXTBUFSGMT_NO_FREE,b,txt,0)
#define SF_HTML_HEADER()     C("<!DOCTYPE html>\n<HTML>\n<HEAD><TITLE>ZIPsFS</TITLE>\n<STYLE> .ms{font-family: monospace;} </STYLE>\n</HEAD>\n<BODY>\n")
#define SF_HTML_END()        C("\n</BODY>\n</HTML>\n")
#define SF_BR()              C("<BR>\n")
#define SF_SYMBOL_B()        C("<TT><FONT color=\"#FF00FF\">")
#define SF_SYMBOL_E()        C("</FONT></TT>")
#define SF_SYMBOL(name)      SF_SYMBOL_B(),C(name),SF_SYMBOL_E()
#define SF_VALUE(name)       C("<TT><FONT color=\"blue\">"),C(name),C("</FONT></TT>")
#define SF_PRINTF(...)  snprintf(tmp,sizeof(tmp)-1,__VA_ARGS__),textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0)
#define SF_FILE_REF(fn) sf_file_ref(b,depth,fn)
#define SF_FILE_ITEM(fn,after) sf_file_item(b,depth,fn,after)

static void shebang_bash(const bool with_mnt,textbuffer_t *b){
  C("#!/usr/bin/env bash\nset -u\n\
export ANSI_BLACK=$'\e[40m'\n\
export ANSI_FG_GREEN=$'\\e[32m' ANSI_FG_RED=$'\\e[31m' ANSI_FG_MAGENTA=$'\\e[35m' ANSI_FG_BLUE=$'\\e[34;1m'\n\
export ANSI_INVERSE=$'\\e[7m' ANSI_BOLD=$'\\e[1m' ANSI_UNDERLINE=$'\\e[4m' ANSI_RESET=$'\\e[0m'\n\
export CTRL_C=Ctrl-C\n\
[[ \"$OSTYPE\" == darwin* ]] && IS_MACOSX=1 && CTRL_C=Command-C\n\
ARGC=$#\n");

  if (with_mnt){C("MNT=");C(_mnt);C("\n");}

}

static void sf_about_atime(textbuffer_t *b){
  if (!_root_writable) return;
  C("echo 'About last-access time (atime)\n==============================\n\nFile systems handle atime differently.\nThe file system of the  first file tree is currently mounted ");
  if (!_root_writable->noatime){
    C("with option noatime.\nZIPsFS will update atime in the past on file reads and the atime mechanism will work perfectly.\n");
  }else{
    C("without option noatime.\nThe OS will update atime on file access. Your atime settings will not persist.\n"
      IF1(IS_MACOSX,"On MacOSX, atime is also updated even when file attributes are read."));
  }
  C("\n'\n");
}

static char *ps1_to_sh(const int id,const char *s){
  static char *ss[99];
  if (!ss[id]){
    char *c=strdup(s);

    if (c) for(char *t=c+1;*t;t++) if (t[-1]=='\n'&&*t=='$') *t=' ';
    ss[id]=c;
  }

  return ss[id];
}


static void  _sf_src_hc(textbuffer_t *b, const enum enum_configuration_src  id,const char *hc){
  SF_SYMBOL_B();
  C(enum_configuration_src_S[id]);
  C(hc);
  SF_SYMBOL_E();
}
#define SF_SRC_H(id) _sf_src_hc(b,id,".h")
#define SF_SRC_C(id) _sf_src_hc(b,id,".c")

static void sf_file_ref(textbuffer_t *b,const int depth, const char *fn){
  const char *parent=NULL;
  RLOOP(i,SFILE_NUM) if (fn==SFILE_NAMES[i]) parent=SFILE_PARENTS[i];
  C("\n<A href=\"");
  FOR(i,0,depth)  C(i?"/..":"..");
  if (parent){C(parent);C("/");}
  C(fn);
  C("\">");
  C(_mnt_apparent);if (parent){C(parent);C("/");};
  C(fn);
  C("</A>\n");
}


static void sf_n_file_ref(textbuffer_t *b, const char *fn){
  C("<LI><A href=\"file:/");
  C(_mnt_apparent); C(DIR_INTERNET"/");C(fn);
  C("\">/");
  C(_mnt_apparent); C(DIR_INTERNET"/");C(fn);
  C("</A></LI>\n");
}


#define SF_REQUIRES(val,name) _sf_requires(b,STRINGIZE(val),name)
static void _sf_requires(textbuffer_t *b,const char *val,const char *name){
  C("This feature is active if the macro ");
  SF_SYMBOL(name);
  C(" is set to 1 in "); SF_SRC_H(ZIPsFS_configuration); C(".\nCurrent value: ");
  C(val);
  C(".<BR>\n");
}

static void sf_requires_rw(textbuffer_t *b){
  C("Furthermore, the first root-path given at the command line must indicate a  file tree with write permission.\nCurrent: ");
  if (_writable_path_l) SF_VALUE(_writable_path); else C("None given at command line");
  SF_BR();
}

static void sf_file_item(textbuffer_t *b,const int depth, const char *fn, const char **after){
  C("<LI>");
  SF_FILE_REF(fn);
  if (after){
    C(" &nbsp; ");
    FOREACH_CSTRING(t,after) C(*t);
  }
  C("</LI>\n");
}
#define SF_CLEANUP() sf_cleanup(b,depth)
static void sf_cleanup(textbuffer_t *b,const int depth){
  C("<H2>Removing old files</H2>\nPreloaded and generated files may be cleaned-up after heaving not been used for a while.\n\
The following script is periodically executed if it exists:<BR>\n");
  SF_VALUE(_cleanup_script); C("\n<BR>Currently, it does ");
  if (!cg_file_exists(_cleanup_script)) C(" not ");
  C(" exist.<BR><BR>\n\
A template of that script file is found in the source code.\n<BR><BR>\n\
Deletion of files can be delayed by setting the <U>last-access-time</U> to the current time or  the future with the following scripts:<UL>\n");
  SF_FILE_ITEM(SFILE_NAMES[SFILE_SET_ATIME_BAT],NULL);
  SF_FILE_ITEM(SFILE_NAMES[SFILE_SET_ATIME_SH],NULL);
  C("</UL>\n");
}

//static void sf_bat_starts_ps1(textbuffer_t *b, const int specialFileId){  C("CLS\n@powershell %~dp0\\");  C(SFILE_NAMES[specialFileId]);}

#define SF_UPDATE() sf_update(b,depth);
static void sf_update(textbuffer_t *b,const int depth){
  C("<H2>Update</H2>\nPreloaded files can be updated by reading the content of the corresponding files in <BR>");
  SF_FILE_REF(DIR_PRELOADED_UPDATE);
  SF_BR();
}




static off_t special_file_size(const int i){
  static int _special_file_size[SFILE_NUM];
  lock(mutex_special_file);
  assert(i>0); assert(i<SFILE_NUM);
  if (!_special_file_size[i]){
    textbuffer_t b={0};
    special_file_content(&b,i);
    _special_file_size[i]=textbuffer_length(&b);
    textbuffer_destroy(&b);
    if (!_special_file_size[i]) _special_file_size[i]=-1;
  }
  unlock(mutex_special_file);
  return _special_file_size[i];
}

static uint64_t special_file_file_content_to_fhandle(zpath_t *zpath,const int special_file_id){
  uint64_t fh=0;
  if (special_file_id || ZPF(ZP_IS_PATHINFO)){
    fHandle_t *d=fhandle_create(FHANDLE_SPECIAL_FILE,&fh,zpath);
    textbuffer_t *b=textbuffer_new(COUNT_MALLOC_PRELOADRAM_TXTBUF);
    cg_thread_assert_not_locked(mutex_fhandle);
    if (ZPF(ZP_IS_PATHINFO)){
      find_realpath(0,zpath);
      char tmp[2*MAX_PATHLEN];
      SF_PRINTF("%s%s%s\n",RP(),EP_L()?"\t":"",EP());
    }else{
      special_file_content(b,special_file_id);
    }
    {
      lock(mutex_fhandle);
      if (!fhandle_set_text(d,b)){
        FREE_NULL_MALLOC_ID(b);
      }else{
        preloadram_set_status(d,preloadram_done);
        d->flags|=FHANDLE_PRELOADRAM_COMPLETE;
      }
      unlock(mutex_fhandle);
    }
  }
  return fh;
}

static void special_file_content_to_file(const int id, const char *path){
  textbuffer_t b={0};
  special_file_content(&b,id);
  const int fd=open(path,O_RDONLY);
  if (fd<0 || textbuffer_differs_from_filecontent_fd(&b,fd,path)){
    log_msg("Going to write %s ...\n",path);
    textbuffer_write_file(&b,path,0770);
  }else{
    log_msg(ANSI_FG_GREEN"Up-to-date %s\n"ANSI_RESET,path);
  }
  if (fd>0) close(fd);
  textbuffer_destroy(&b);
}

// /var/Users/cgille/tmp/ZIPsFS/mnt/zipsfs/n/_UPDATE_/ftp,,,ftp.expasy.org,databases,uniprot,current_release,knowledgebase,reference_proteomes,Eukaryota,UP000000589,UP000000589_10090.fasta.html
static void html_is_uptodate(fHandle_t *d,const yes_zero_no_t updateSuccess, zpath_t *zpath, const char *pathOrig,const struct stat *stOrig, const char *relPath){
  struct stat  *st1=&zpath->stat_rp;
  textbuffer_t *b=textbuffer_new(COUNT_MALLOC_PRELOADRAM_TXTBUF);
  SF_HTML_HEADER();
#define d "\t</TD>"
#define H(s) "<TH>" s "\t</TH>"
#define D  "<TD class=\"ms\">"
#define R(size,style,status) "<TR>"D"%s"d D"%s"d"<TD class=\"ms\" style=\"text-align: right;\">"size d"<TD" style ">"status d "</TR>\n"

  //"%#010x"

  C("<TABLE  style=\"border: 1px solid black;border-collapse: collapse;\">\n<TR>"H("Path")H("Modified")H("Size")H("Status")"</TR>\n");
  char tmp[strlen(pathOrig)+RP_L()+4096];
  SF_PRINTF( R("%'lld","","Original"),pathOrig,ST_MTIME(stOrig),(LLD)stOrig->st_size);
  const char *s_ne="Not exist", *s_utd="Up to date", *s_ufail="Update failed", *s_usuccess="Update succeeded", *s_uneed="Need_update";
#define TROW_LOADED(s) SF_PRINTF(R("%'lld"," style=\"color:#%06x;\"","%s"), RP(),st1->st_mtime?ST_MTIME(st1):"",	\
                                 (LLD)st1->st_size,	\
                                 s==s_ne || s==s_ufail?0xFF: s==s_utd||s==s_usuccess?0xFF00: s==s_uneed?0xFF00FF: 0,\
                                 s);

  const char *s=(!st1->st_ino?s_ne: updateSuccess==ZERO?s_utd: updateSuccess==NO?s_ufail:  s_uneed); TROW_LOADED(s);
  if (updateSuccess==YES){
    *st1=empty_stat;
    stat(RP(),st1);
    s=stOrig->st_mtime>st1->st_mtime?s_usuccess:s_ufail; TROW_LOADED(s);
  }
  SF_PRINTF( R("","","Relative path"),st1->st_ino?relPath:"Not-exist","");
#undef TROW_LOADED
#undef D
#undef d
#undef H
#undef R
  C("</TABLE>\n");
  SF_HTML_END();
  //textbuffer_write_fd(b,STDERR_FILENO);
  fhandle_set_text_or_free(d,b);
}



// Testing: mnt/zipsfs/_UPDATE_PRELOADED_FILES_/txt/numbers.txt.htmL     mnt/zipsfs/_UPDATE_PRELOADED_FILES_/DB/ebi/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.htmL
//////////////////////////////////////////////////////
/// Generate virtual files with immutable content. ///
//////////////////////////////////////////////////////
static void sf_begin_ps1(textbuffer_t *b){
  C("$CTRL_C='Ctrl-C'\n$ARGC=$($args.Count)\n");
}


static void special_file_content(textbuffer_t *b,const enum enum_special_files id){
  char tmp[333];
  if (id==SFILE_CLEAR_CACHE || id==SFILE_DEBUG_CTRL){
    shebang_bash(true,b);
    SF_PRINTF("CTRL_SFX=%s\n",ctrl_file_end());
    FOR(a,1,enum_ctrl_action_N) SF_PRINTF("%s=%d\n",enum_ctrl_action_S[a],a);
    static const char *c=
#include "tmp/include_ctrl_common.sh.c"
      C(c);
  }

  static const char *common_begin_net_fetch=
#include "tmp/include_net_fetch_common.ps1.c"



    static const char *how_select_files="\n\
function how_select_files(){\n\
echo \"\n\
Howto select files\n\
==================\n\
Unless files are given as command line parameters (currently $ARGC).\n\
the user is asked to select files in the file browser.\n\
Files can be selected  in the file browser and copied to the clipboard using $CTRL_C.\n\
\"\n\
}\n\n";
  static const char *sh_copied_paths=
#include "tmp/include_copied_paths.sh.c"




    static const char *begin_set_atime="echo '\
*********************************\n\
***  Setting last access time ***\n\
*********************************\n\n\
With this script, the last last-access time of writable files in ZIPsFS can be changed to now, the past or the future.\n	\
\n\
Motivation\n\
==========\n\
By setting the last-access time (atime), removal of generated  files by the cleanup script can be avoided.\n\n\
This works only for files in the first file tree which is the writable one.\n'\n\
function ask_how_long_ago(){\n\
echo '\n\
Enter a number of hours to be added to current time.\n\
Type 0 to set last-access-time to the current time.\n\
Type a negative number to pretend, that files were accessed in the past. Files will be cleaned up earlier.\n\
Type a positive number to pretend that files were last accessed in the future. This will extend the life span of the files\n\
With suffix d, w, m and y, days, weeks, month and years can be given. For example 3y means three years.\n\n\'\n\
}\n\
$NO_FF='No files given. Please select files in the file browser.'\n\
$SFX_ATIME=\"" MAGIC_SFX_SET_ATIME "\"\n\
function ask_not_existing(){ echo 'There may be  virtual files that do not exist as real local files yet like remote files or converted  files.'; }\n\
$ASK_NOT_EXISTING='Generated or download those files? [y/N]'",

    *begin_read_beginning="\
echo This script reads the first lines of files\n\
echo .\n\
echo USE-CASE\n\
echo --------\n\
echo Forces  generation of converted files or  downloading of remote files.\n\
echo After this, the file size will be known and not estimated any more.\n\
echo .\n\
echo INSTRUCTION\n\
echo -----------\n\
echo Select files in the file browser.\n\
echo Copy the selected files to the clipboard.  Ctrl-C or Command-C.\n";
  const int depth=cg_count_chr(SFILE_PARENTS[id],'/');
  switch(id){
  case SFILE_DEBUG_CTRL:{
    C("tt=(");
    FOR(t,1,enum_root_thread_N){ C("'");C(enum_root_thread_S[t]);C("' ");}
    C(")\n");
    static const char *c=
#include "tmp/include_ctrl.sh.c"
      ;C(c);}
    break;
  case SFILE_CLEAR_CACHE:{
    C("menu(){\n");
    FOR(a,0,enum_clear_cache_N) SF_PRINTF(" echo '%d  %s'\n",a,enum_clear_cache_S[a]);
    C("}\n");
    static const char *c=
#include "tmp/include_clear_cache.sh.c"
      ;C(c);}
    break;
  case SFILE_README:
    SF_HTML_HEADER();
    C("<H1>The ZIPsFS FUSE file system.</H1><H2>Introduction</H2>\n\
This is the virtual file system ZIPsFS. Main features:<BR>\
<UL>\n\
<LI>It can expand ZIP files</LI>\n\
<LI>Acting as a union file system, it can combine two or more file trees.</LI>\n\
<LI>Preloading of file content to disk improves file reading. Use cases: <UL>\n\
 <LI>Reading from varying file positions (i.e. file seek) from compressed storages</LI>\n\
 <LI>Reading from remote sites with long transfer times</LI>\n\
 <LI>Laptop and Using remote files offline</LI>\n\
</UL></LI>\n\
<LI>Preloading of file content to RAM improves file reading and  allows software to directly work on archived data without prior un-zipping.\n\
Preloading is decided based on file name and size in "); SF_SRC_C(ZIPsFS_configuration);
    C(".\nUse cases: Like for preloading to disk if each remote file is accessed not more than once.</LI>\n\
<LI>Protection of files. Only files in the first file tree can be modified or deleted.</LI>\n\
<LI>Files and folders look writable and modified files are stored in the first file tree.\nUse cases:<UL>\n\
<LI>Software using  wrong flags for opening files for reading which is often the case for software based on native Windows funcions  rather than POSIX.</LI>\n\
<LI>Software writing output files into the parent folders of the input files.</LI>\n\
</UL></LI>\n\
<LI>Programatically generated files. See "); SF_SRC_C(ZIPsFS_configuration_c); C("</LI>\n\
<LI>Reshaping of directory structures to match Sciex mass spectrometry. See "); SF_SRC_C(ZIPsFS_configuration_zipfile); C("</LI>\n\
<LI>Performance optimizations<UL>\n\
<LI>Caches for directory entries and ZIP file content. See "); SF_SRC_H(ZIPsFS_configuration); C("</LI>\n\
<LI>Removing files from page cache after usage</LI>\n\
</UL></LI>\n\
</UL>\n\
<A href=\"" HOMEPAGE "\">Homepage</A><BR>\n\
<H2>File-conversion</H2>\n"); SF_REQUIRES(WITH_FILECONVERSION,"WITH_FILECONVERSION");
    C("Generated files are displayed in the file tree <B>");   C(_mnt_apparent);C(DIR_FILECONVERSION);C("/</B>.\n\
To prevent recursive file searches this folder can be hidden by setting the macro  <B>WITH_FILECONVERSION_HIDDEN</B> to <B>1</B>.\nCurrent value: ");
    SF_SYMBOL(STRINGIZE(WITH_FILECONVERSION_HIDDEN)); C(". Also see "); SF_FILE_REF(SFILE_NAMES[SFILE_README_FILECONVERSION]);
    C("\n<H2>File-preloading</H2>\n\
Fore remote or compressed files, preloading of file content makes file reading more efficient.\n\
Also see ");    SF_FILE_REF(SFILE_NAMES[SFILE_README_PRELOADDISK_R]);
    C("\n<H2>Files from the internet</H2>\nFiles from http web pages or FTP servers can be accessed directly through the filesystem. See ");
    SF_FILE_REF(SFILE_NAMES[SFILE_README_INTERNET]);
    C(" for details.");
    SF_HTML_END();
    break;
#define L(d) C("Files will be copied into "ANSI_FG_BLUE);C(_mnt_apparent); C(_writable_path);C(d ANSI_RESET".\n")
  case SFILE_README_PRELOADDISK_R:
  case SFILE_README_PRELOADDISK_RC:
  case SFILE_README_PRELOADDISK_RZ:
  case SFILE_README_PRELOADDISK_UPDATE:
    SF_HTML_HEADER();
    C("<H1>Preloading files to improve efficiency of file reading</H1>\nZIPsFS can cache file content on the local disk to improve file reading in the following cases:\n\
<UL>\
<LI>Repeated access of remote files.</LI>\n\
<LI>Random-access at varying file positions of remote or compressed files.</LI>\n\
</UL>\n");
    SF_REQUIRES(WITH_PRELOADDISK,"WITH_PRELOADDISK");
    sf_requires_rw(b);
    C("<H2>Mark entire file branch</H2>\nAt the command line, root paths can be followed by the property "); C(ROOT_PROPERTY[ROOT_PROPERTY_preload]);
    C(".\nThis is useful for remote sites. Furthermore the properties the following properties can indicate to decompress files that have a compression suffix:\n");
    SF_SYMBOL_B();
    FOR(iCompress,COMPRESSION_NIL+1,COMPRESSION_NUM){ C(" &nbsp;  @"); C(ROOT_PROPERTY[ROOT_PROPERTY_preload]);C(cg_compression_file_ext(iCompress,NULL));}
    SF_SYMBOL_E();
    SF_BR();
    SF_UPDATE();
    C("<H2>Special directories</H2>\nAnother option to force preloading of files is to access the files through dedicated paths:<UL>\n");
#define D(dir, condition) C("  - "ANSI_FG_BLUE);C(_mnt_apparent);C(dir"/"ANSI_RESET"  "condition"\n")
    const char *after[4]={0};
    after[0]="Remote root";         SF_FILE_ITEM(DIR_PRELOADDISK_R,after);
    after[1]="or compressed file";  SF_FILE_ITEM(DIR_PRELOADDISK_RC,after);
    after[2]="or zipped file";      SF_FILE_ITEM(DIR_PRELOADDISK_RZ,after);
    C("</UL>\n");
    SF_UPDATE();
    SF_CLEANUP();
    SF_HTML_END();
#undef D
#undef L
    break;
   case SFILE_README_SERIALIZED:
     C("Simultaneous reading of many files from harddisks may be inefficient\nand puts strain on hardware due to extensive head movements.\n\
For files read from this location, only one thread can read  per root/upstream source at a time.\n\
Serialized file access may be more efficient. Cave dead-locks!");
     break;
   case SFILE_README_PREFETCH_RAM:
     C("All files from this folder or subfolders  will be prefetched into RAM. This may facilitate none-sequential reading files from several threads and from varying positions.");
     break;
  case SFILE_README_PLAIN:
    C("This directory provides plain access to all files.\n	  - not expanding ZIP files.\n\n\nUseful for rapid navigation and searching.\n\n");
    break;
  case SFILE_README_FIRST_ROOT:
    C("This directory contains only those files that reside in the first branch which is the writable branch.\n\
It may be used to selectively process files that are generated or downloaded from remote locations.\n");
  case SFILE_README_LOGGING:{
    C("File access is logged in file "); C(SFILE_REAL_PATHS[SFILE_LOG_FUNCTION_CALLS]);	C(".\n\
The logs can be used to identify misbehaving software which should rather be used with preloading for remote or compressed files.\n\
  - Excessive requests of file attributes\n\
  - Multiple open/close\n\
  - Backward seek\n\
  - Upper/lower case conversion of file names\n\n\
Run "); C(SFILE_NAMES[SFILE_LOGGING_COMMAND]);C(" to watch or view the logs.\n");
  }break;
  case SFILE_LOGGING_COMMAND:{
    static const char *awk=
#include "tmp/include_print_tsv.awk.c"
      shebang_bash(false,b);
    C("f=");C(SFILE_REAL_PATHS[SFILE_LOG_FUNCTION_CALLS]);
    C("\n[[ ! -f $f ]] && echo 'Not a file '$f && f=${0%/*}/");C(SFILE_NAMES[SFILE_LOG_FUNCTION_CALLS]);
    C("\n""my_format(){\n awk '"); C(awk); C("'\n}\n\
[[ -s $f ]] && case ${1:-} in\n	\
-p) my_format <$f |less;;\n\
-l) tail -f $f |my_format;;\n\
*) nl $f;;\n\
esac\n\
ls -l -d $f\n\
echo -e \"Options:\n   $0 -p  Display in pager\n   $0 -l  Continuous logging\"\n\
");
  }break;
  case SFILE_README_EXCLUDE_FIRST_ROOT:
    C("This directory contains only those files that reside in NOT the first branch which is the writable branch.\n");
    break;
  case SFILE_README_INTERNET_UPDATE:
    C("<H1>Updating internet files</H1>\nBy reading at least one byte, files will be downloaded again if older than the source in the internet.");
  case SFILE_README_INTERNET:
    SF_HTML_HEADER();
    C("<H1>Accessing files from the internet</H1>\n");
    SF_REQUIRES(WITH_INTERNET_DOWNLOAD,"WITH_INTERNET_DOWNLOAD");
    sf_requires_rw(b);
    C("The directory "); SF_FILE_REF(DIR_INTERNET);
    C("provides access to internet files.\n\
The file names are formed from URLs by replacing  the colon and slashes by comma.\n\
ZIPsFS automatically downloads and updates the files.\n\
The times of documents are taken from the http or ftp headers which are stored in  separate files.\n\
The utility curl must be installed on the computer.\n\n\
<U>Examples:</U>\n");
    sf_n_file_ref(b,"ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE");
    sf_n_file_ref(b,"https,,,ftp.uniprot.org,pub,databases,uniprot,README");
    sf_n_file_ref(b,"https,,,files.rcsb.org,download,1SBT.pdb");
    sf_n_file_ref(b,"ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml");
    sf_n_file_ref(b,"ftp,,,ftp.uniprot.org,pub,databases,uniprot,previous_releases,release-2022_05,reference_proteomes,Eukaryota,UP000005640,UP000005640_9606_canonical.fasta");
    C("</UL>\n\
The last file is only available as a gz compressed file. When the '<TT>.gz</TT>' suffix is omitted, the file will be decompressed during data transfer.\n\
Before decompressing tranfer, the file size is estimated.");
    SF_HTML_END();
    break;
  case SFILE_NET_FETCH_BAT:
    sf_begin_ps1(b);
    C("CLS\n%~dpn0.ps1\n");

    //	sf_bat_starts_ps1(b,SFILE_NET_FETCH_PS);
    break;
  case SFILE_NET_FETCH_PS:{
    C(common_begin_net_fetch);
    static const char *s=
#include "tmp/include_net_fetch.ps1.c"
      ;C(s);
  }	break;
  case SFILE_NET_FETCH_SH:{
    shebang_bash(false,b);
    C(ps1_to_sh(SFILE_NET_FETCH_SH,common_begin_net_fetch));
    static const char *c=
#include "tmp/include_net_fetch.sh.c"
      C(c);
  }	break;
  case SFILE_README_FILECONVERSION:
    SF_HTML_HEADER();
    C("<H1>File-conversion - Files generated automatically from other files</H1>\n");
    SF_REQUIRES(WITH_FILECONVERSION,"WITH_FILECONVERSION");
    sf_requires_rw(b);
    C("<BR>The file tree "); SF_FILE_REF(DIR_FILECONVERSION);  C(" replicates the virtual file tree.\n\
In addition it presents dynamically generated files. The files come into existence only when used.\n\
They will be created using comands specified in "); SF_SRC_C(ZIPsFS_configuration_fileconversion); C(".<BR>\n\
<H2>Initiate file generation</H2>\n\
To force file generation, it is sufficient to read at least one byte of the file.  UNIX commands like like the following can be used:\n<PRE>\n\n\
    head * | strings\n\n\
</PRE>\
For Windows, the  script "); SF_FILE_REF(SFILE_NAMES[SFILE_READ_BEGINNING_OF_FILES_BAT]); C(" is available.<BR>\n\
<H2>Unknown file size</H2>\n\
As long as the file content has not been  generated, the file size is guessed. \n\
It should be an overestimation to avoid premature end-of-file.<BR>\n");
    SF_CLEANUP();
    SF_HTML_END();
    break;
  case SFILE_SET_ATIME_SH:{
    shebang_bash(false,b);
    C(ps1_to_sh(SFILE_SET_ATIME_SH,begin_set_atime));
    C(how_select_files);
    sf_about_atime(b);
    C(sh_copied_paths);
    static const char *s=
#include "tmp/include_set_atime.sh.c"
      C(s);
  }break;
  case SFILE_SET_ATIME_PS:{
    C(how_select_files);
    C(begin_set_atime);
    static const char *s=
#include "tmp/include_set_atime.ps1.c"
      C(s);
  }break;
  case SFILE_SET_ATIME_BAT:
    //	sf_bat_starts_ps1(b,SFILE_SET_ATIME_PS);
    C("CLS\n%~dpn0.ps1\n");
    break;
  case SFILE_READ_BEGINNING_OF_FILES_SH:{
    shebang_bash(false,b);
    C(begin_read_beginning);
    C(how_select_files);
    C(sh_copied_paths);
    static const char *s=
#include "tmp/include_head.sh.c"
      C(s);
  } break;
  case SFILE_READ_BEGINNING_OF_FILES_BAT:
    C("ECHO OFF\nCLS\n");
    C(begin_read_beginning);
    C("@pause\n\
@powershell -NoProfile -ExecutionPolicy Bypass -c \"get-clipboard -format FileDropList|Get-ChildItem -File |%%{ Write-Host '=== '$_.FullName' ===' -ForegroundColor Cyan; gc $_ -Encoding String -TotalCount 3}\"\n\
@pause\n");
    break;
  default:;
  }
#undef T
#undef I
#undef P
#undef B
}

#undef C
#undef SF_REQUIRES
#undef SF_PRINTF
