// (defun R() (interactive) (query-replace-regexp "case ID_(\\(\\w+\\)):" "CASE_ID1(\\1);"))
/************************************************************************/
/* COMPILE_MAIN=ZIPsFS                                                  */
/* Special generated files                                              */
/************************************************************************/
_Static_assert(WITH_PRELOADRAM,"");
#define C(txt) textbuffer_add_segment(TXTBUFSGMT_NO_FREE,b,txt,0)

static void sf_html_header(textbuffer_t *b){
  C("<!DOCTYPE html>\n<HTML>\n<HEAD><TITLE>ZIPsFS</TITLE>\n<STYLE> .ms{font-family: monospace;} </STYLE>\n</HEAD>\n<BODY>\n");
}

static void sf_html_end(textbuffer_t *b){
  C("\n</BODY>\n</HTML>\n");
}
#define SF_HTML_HEADER()     if (!_print_doc) sf_html_header(b);isHTML=true
#define SF_HTML_END()        if (!_print_doc) sf_html_end(b)
#define SF_BR()              C("<BR>\n")
#define SF_SYMBOL_B()        C("<TT><FONT color=\"#FF00FF\">")
#define SF_SYMBOL_E()        C("</FONT></TT>")
#define SF_SYMBOL(name)      SF_SYMBOL_B(),C(name),SF_SYMBOL_E()
#define SF_VALUE_B()       C("<TT><FONT color=\"blue\">")
#define SF_VALUE_E()       C("</FONT></TT>")

#define SF_VALUE(name)       SF_VALUE_B(),C(name),SF_VALUE_E()
#define SF_PRINTF(...)  snprintf(tmp,sizeof(tmp)-1,__VA_ARGS__),textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0)


#define SF_H1(headline)   C("\n<H1>"); C(headline);C("</H1>\n");
#define SF_TELL_DIRFLAG(dirflag) if (id==ID_(dirflag)) sf_tell_dirflag(b,ID_(dirflag),#dirflag)
#define CASE_ID1(id,headline) case ID_(id):SF_HTML_HEADER();SF_H1(headline);SF_TELL_DIRFLAG(id)
#define CASE_ID2(id1,id2,headline) case ID_(id1):case ID_(id2):                  if(_print_doc&&id!=ID_(id1)) break; SF_HTML_HEADER();SF_H1(headline);SF_TELL_DIRFLAG(id1);SF_TELL_DIRFLAG(id2)
#define CASE_ID3(id1,id2,id3,headline) case ID_(id1):case ID_(id2):case ID_(id3):if(_print_doc&&id!=ID_(id1)) break; SF_HTML_HEADER();SF_H1(headline);SF_TELL_DIRFLAG(id1);SF_TELL_DIRFLAG(id2);SF_TELL_DIRFLAG(id3)
static const char *sf_equivalent_root_property(const int id){
  const int p=
    id==ID_(PRELOADDISK) || id==ID_(PRELOADDISK_NOT)?ROOT_PROPERTY_preload:
    id==ID_(PRELOADDISK_ENTIRE_ZIP)?ROOT_PROPERTY_preload_entire_zip:
    -1;
  return p==-1?NULL:ROOT_PROPERTY[p];
}

static void sf_tell_dirflag(textbuffer_t *b,const int id, const char *name){
  //if (_specialfiles[id].name) return;
  if (id>=VFOLDER_NUM || id==ID_(PRELOAD_UPDATE) || id==ID_(VIEWMOD_NIL) || id==ID_(VIEWMOD_INTERNET)) return;
  const int category=
    num_in_range(id,ID_RANGE_VIEWMOD)?1:
    num_in_range(id,ID_RANGE_PRELOAD)?2:
    num_in_range(id,ID_RANGE_PRELOAD_SELECT)?3:0;
  assert(category);

  C("The "); C(category==1?"view option ":category==2?"preload option ":"preload selector ");SF_VALUE(name); C(" is activated with the flag ");SF_VALUE(_virtualfolder_dirflags[id]);
  C(" in the ");
  SF_VALUE(category==1?"first":category==2?"second":"third");
  C(" subfolder in "DIR_ZIPsFS"/.");
  if (id==ID_(PRELOADDISK)||id==ID_(PRELOADDISK_ENTIRE_ZIP)||id==ID_(VIEWMOD_1)){
    C("It can be deactivated with ");SF_VALUE_B();C("-");SF_VALUE(_virtualfolder_dirflags[id]);SF_VALUE_E();
    C(".\n");
  }
  const char *rp=sf_equivalent_root_property(id),*flag=_virtualfolder_dirflags[id];
  const char *stronger=id==ID_(PRELOADRAM)?"configuration.\n":NULL;
  if (rp){
    C("This flag is equivalent to the root tree property ");
    SF_VALUE_B();C(rp); if (flag && *flag=='-') C("=0"); SF_VALUE_E(); C(" ");
    stronger="file tree properties.\n";
  }
  if (stronger) {C(" Flags take precedence over ");C(stronger);C(".");}
  C("<BR><BR>\n");
}

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
static void  sf_src(textbuffer_t *b, const enum_sourcefiles_t  id){
  SF_SYMBOL_B();
  C(enum_sourcefiles_S[id]);
  SF_SYMBOL_E();
}

#define SF_SRC_C(id) sf_src(b,id##_dot_c)
#define SF_SRC_H(id) sf_src(b,id##_dot_h)
#define SF_SRC_CH(id) sf_src(b,id##_dot_h);C(" and ");sf_src(b,id##_dot_c)

#define SF_SRC_SH(id) sf_src(b,id##_dot_sh)
static void sf_ref5(textbuffer_t *b, const char *path1, const char *path2, const char *path3, const char *path4, const char *path5){
#define A()  if (cg_path_equals_or_is_parent(DIR_ZIPsFS,sizeof(DIR_ZIPsFS)-1,path1,cg_strlen(path1))) C(_mnt_apparent); C(path1); C(path2);C(path3);C(path4);C(path5);
  C("\n<A href=\"");A();C("\">");A();C("</A>\n");
#undef A
}
#define SF_REF(path)            sf_ref5(b,path,NULL,NULL,NULL,NULL)
#define SF_REF_DIR_NET(fn)      sf_ref5(b,DIR_INTERNET,"/",fn,NULL,NULL);
#define sf_ref_special_file(b,id)  _sf_ref_special_file(b,id,true)
#define sf_ref_special_dir(b,id)   _sf_ref_special_file(b,id,false)
static void _sf_ref_special_file(textbuffer_t *b,const int id, const bool withName){
  const specialfile_t *sf=_specialfiles+id;
  if (sf->name && sf->parent){
    sf_ref5(b,sf->parent,sf->name,NULL,NULL,NULL);
  }else{
    const char *parent2=NULL, *parent1=
      num_in_range(id,ID_RANGE_VIEWMOD)?DIR_ZIPsFS"/":
      num_in_range(id,ID_RANGE_PRELOAD)?DIR_ZIPsFS"/-/":
      num_in_range(id,ID_RANGE_PRELOAD_SELECT)?DIR_ZIPsFS"/-/l/":
      NULL;
    if (parent1) parent2=_virtualfolder_dirflags[id];  else parent1=sf->parent;
    sf_ref5(b,_mnt_apparent,parent1,parent2,withName?"/":NULL,withName?sf->name:NULL);
  }
}
#define SF_REQUIRES(id) _sf_requires(b,id,#id)
static void _sf_requires(textbuffer_t *b,const int val,const char *name){
  if (_print_doc){ C("This feature is activated with the macro ");SF_VALUE(name);C(".<BR>\n"); return;}
  C("This feature is "); C(val?"enabled":"disabled"); C(" because the macro ");
  SF_SYMBOL(name);  C(" has the value  ");SF_VALUE(val?"1.<BR>\n":"0.<BR>\n");
}
#define SF_REQUIRES_W()  sf_requires_w(b);
static void sf_requires_w(textbuffer_t *b){
  if (_print_doc){ C("This feature requires the writable file tree. ");return;}
  if (_writable_path_l){
    C("Furthermore, a writable path tree is provided: "); SF_VALUE(_writable_path); C(". ");
  }else{
    C("This feature is inactive because no  writable path tree is provided.");
  }
  SF_BR();
}
static void sf_cleanup(textbuffer_t *b){
  const specialfile_t *sf=_specialfiles+SFILE_CLEANUP_SH;
  C("<H2>Removing old files</H2>\nPreloaded and generated files may be cleaned-up after heaving not been used for a while.\n\
The following script is periodically executed if it exists:<BR>\n");
  SF_VALUE_B();
  if (sf->rp){ C(sf->rp); }else{ C("&lt;writable root&gt;/");C(sf->parent);C(sf->name); }
  SF_VALUE_E();
  C("\n<BR>Currently, it does ");
  if (!cg_file_exists(sf->rp)) C(" not ");
  C(" exist.<BR><BR>\n\
A template  script "); SF_SRC_SH(ZIPsFS_cleanup); C(" is found in the source code.\n<BR><BR>\n\
Deletion of files can be delayed by setting the <U>last-access-time</U> to the current time or  the future with the following scripts:<UL>\n<LI>");
  sf_ref_special_file(b,SFILE_SET_ATIME_SH);
  C("</LI>\n</UL>\n");
}

//static void sf_bat_starts_ps1(textbuffer_t *b, const int specialFileId){  C("CLS\n@powershell %~dp0\\");  C(_specialfiles[specialFileId].name);}
static void sf_update(textbuffer_t *b){
  C("<H2>Update</H2>\nPreloaded files can be updated by reading the content of the corresponding HTML files in the "); SF_VALUE(DIRNAME_PRELOADDISK_UPDATE);C(" folder. <BR>");
  SF_BR();
}
static off_t specialfile_size(const int i){
  if (i==ID_(PRELOAD_UPDATE) || i==ID_(INTERNET_UPDATE)) return 4096;
  static int _special_file_size[SFILE_NUM];
      lock(mutex_specialfile);
      if (!_special_file_size[i]){
        textbuffer_t b={0};
        specialfile_content(&b,i);
        _special_file_size[i]=textbuffer_length(&b);
        textbuffer_destroy(&b);
        if (!_special_file_size[i]) _special_file_size[i]=-1;
      }
  unlock(mutex_specialfile);
  return _special_file_size[i];
}


static int specialfile_print_pathinfo(zpath_t *zpath,textbuffer_t *b_or_null){
  zpath_stat(0,zpath);
  if (zpath->vfile_sfx) log_entered_function("%s %ju",VP(),UIM(zpath->stat_rp.st_ino));
  if (!zpath->vfile_sfx || !zpath->stat_rp.st_ino && !find_realpath(0,zpath)) return 0;
  const int tmp_max=zpath->vfile_sfx==VFILE_SFX_ZIPCRC32?9:zpath->vfile_sfx==VFILE_SFX_SOURCE? RP_L()+EP_L()+2:4096;
  char tmp[tmp_max];
  int l=0;
  const bool isP=zpath->vfile_sfx==VFILE_SFX_PROPERTIES;
#define S(...) {if (l<tmp_max) l+=snprintf(tmp+l,tmp_max-l,__VA_ARGS__);}
  if (zpath->vfile_sfx==VFILE_SFX_ZIPCRC32 || isP){
    S("%s%08x\n",isP?"Archive-CRC32:\t":"",zpath->zipcrc32);
  }
  if (zpath->vfile_sfx==VFILE_SFX_SOURCE || isP){
    S("%s%s%s%s\n",isP?"Upstream file:\t":"",RP(),EP_L()?"\t":"",EP());
  }
  if (isP){
    S("Directives in folder path:\t");
    l+=virtualfolders_print_dirflags(tmp+l,tmp_max-l,VFOLDER_FLAGS(zpath));
    l+=root_property_print(OUTPUT_TEXT,zpath->root,tmp+l,tmp_max-l);
  }
#undef S
  if (l>0 && b_or_null) textbuffer_add_segment(TXTBUFSGMT_DUP,b_or_null,tmp,0);
  return l;
}
static uint64_t specialfile_content_to_fhandle(zpath_t *zpath,const int id){
  //log_entered_function("%s  SFILE_IS_IMMUTABLE=%d  zpath->vfile_sfx=%s",VP(),SFILE_IS_IMMUTABLE(id),zpath->vfile_sfx);
  if (!SFILE_IS_IMMUTABLE(id) && !zpath->vfile_sfx) return 0;
  uint64_t fh=0;
  fHandle_t *d=fhandle_create(FHANDLE_SPECIAL_FILE,&fh,zpath);
  cg_thread_assert_not_locked(mutex_fhandle);
  textbuffer_t *b=textbuffer_new(COUNT_MALLOC_PRELOADRAM_TXTBUF);
  if (zpath->vfile_sfx){
    specialfile_print_pathinfo(zpath,b);
  }else{
    specialfile_content(b,id);
  }
      lock(mutex_fhandle);
      if (!fhandle_set_text(d,b)){
        FREE_NULL_MALLOC_ID(b);
      }else{
        preloadram_set_status(d,PRELOADRAM_DONE);
        d->flags|=FHANDLE_PRELOADRAM_COMPLETE;
      }
  unlock(mutex_fhandle);
  //log_exited_function("%s id=%d  '%s'  fh=%jd  length:%ld",VP(),id,_specialfiles[id].name, IM(fh), textbuffer_length(b));
  return fh;
}

static void specialfile_content_to_file(const int id, const char *path){
  textbuffer_t b={0};
  specialfile_content(&b,id);
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
static void html_is_uptodate(fHandle_t *d,const yes_zero_no_t updateSuccess,const char *rp, const struct  stat *st1,  const char *pathOrig,const struct stat *stOrig, const char *relPath){
  //log_entered_function("%s  updateSuccess:%d",RP(),updateSuccess);
  //struct stat *st1=&zpath->stat_rp;
  const int rp_l=strlen(rp);
  textbuffer_t *b=textbuffer_new(COUNT_MALLOC_PRELOADRAM_TXTBUF);
  bool isHTML=false;
  SF_HTML_HEADER();
#define d    "\t</TD>"
#define H(s) "<TH>" s "\t</TH>"
#define D    "<TD class=\"ms\">"
#define R(size,style,status) "<TR>"D"%s"d D"%s"d"<TD class=\"ms\" style=\"text-align: right;\">"size d"<TD" style ">"status d "</TR>\n"
  C("<TABLE  style=\"border: 1px solid black;border-collapse: collapse;\">\n<TR>"H("Path")H("Modified")H("Size")H("Status")"</TR>\n");
  char tmp[strlen(pathOrig)+rp_l+4096]; DATETIME_BUF();
  SF_PRINTF( R("%'jd","","Original"),pathOrig,DATETIME_COLON(stOrig->st_mtime),IM(stOrig->st_size));
  const char *s_ne="NOT_EXIST", *s_utd="UP_TO_DATE", *s_ufail="UPDATE_FAILED", *s_usuccess="UPDATE_SUCCEEDED", *s_uneed="UPDATE_NEEDED";
#define TROW_LOADED(s) SF_PRINTF(R("%'jd"," style=\"color:#%06x;\"","%s"), rp,st1->st_mtime?DATETIME_COLON(st1->st_mtime):"",\
                                 IM(st1->st_size),\
                                 s==s_ne || s==s_ufail?0xFF: s==s_utd||s==s_usuccess?0xFF00: s==s_uneed?0xFF00FF: 0,\
                                 s);
  const char *s=(!st1->st_ino?s_ne: updateSuccess==ZERO?s_utd: updateSuccess==NO?s_ufail:  s_uneed); TROW_LOADED(s);
  struct stat st2={0};
  if (updateSuccess==YES){
    stat(rp,&st2);
    s=stOrig->st_mtime>=st2.st_mtime?s_usuccess:s_ufail; TROW_LOADED(s);
  }
  SF_PRINTF( R("","","Relative path"),(updateSuccess==YES?&st2:st1)->st_ino?relPath:"Not-exist","");
#undef TROW_LOADED
#undef D
#undef d
#undef H
#undef R
  SF_PRINTF("</TABLE>\n<!-- Extract tsv with grep $'\t'\n%s\t%s\n-->\n",s,relPath);
  SF_HTML_END();
  fhandle_set_text_or_free(d,b);
}



// Testing: mnt/zipsfs/_UPDATE_PRELOADED_FILES_/txt/numbers.txt.htmL     mnt/zipsfs/_UPDATE_PRELOADED_FILES_/DB/ebi/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.htmL
//////////////////////////////////////////////////////
/// Generate virtual files with immutable content. ///
//////////////////////////////////////////////////////
static void specialfile_content(textbuffer_t *b,const int id){
  //log_entered_function("id:%d",id);
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
    ;
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
    ;
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
  bool isHTML=false;
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
  case SF_README_FOR_DIR(DIR_ZIPsFS): SF_HTML_HEADER();
    C("<H1>This folder</H1>\nThe special folder ");
    SF_VALUE_B();C(_mnt_apparent); C(DIR_ZIPsFS); SF_VALUE_E();
    C(" of the fuse file system ZIPsFS contains logs and file system information. Furthermore there are folders to display  file trees in a different way.\n\
<H2>Main features of the fuse file system ZIPsFS</H2>\n\
<UL>\n\
<LI>It can expand ZIP files</LI>\n\
<LI>Acting as a union file system, it can combine two or more file trees.</LI>\n\
<LI>Preloading of file content to disk improves file reading. See  ");sf_ref_special_file(b,ID_(PRELOADRAM));C("</LI>\n\
<LI>Preloading of file content to RAM improves file reading and allows software to directly work on archived data without prior un-zipping.  See ");sf_ref_special_file(b,ID_(PRELOADDISK)); C("</LI>\n\
<LI>Protection of files. Only files in the first file tree can be modified or deleted.</LI>\n\
<LI>Files and folders look writable and modified files are stored in the first file tree.\nUse cases:<UL>\n\
<LI>Software using  wrong flags for opening files for reading which is often the case for software based on native Windows funcions  rather than POSIX.</LI>\n\
<LI>Software writing output files into the parent folders of the input files.</LI>\n\
</UL></LI>\n\
<LI>File conversions and  programatically generated files. Folder ");sf_ref_special_dir(b,ID_(VIEWMOD_FILECONVERSION));C(" See configuration ");SF_SRC_C(ZIPsFS_configuration); C(" and ");SF_SRC_C(ZIPsFS_configuration_fileconversion); C("</LI>\n\
<LI>Reshaping of directory structures to match Sciex mass spectrometry. See "); SF_SRC_C(ZIPsFS_configuration_zipfile); C("</LI>\n\
<LI>Accessing files from the internet like normal files See ");   sf_ref_special_file(b,ID_(VIEWMOD_INTERNET));C("</LI>\n\
<LI>Performance optimizations<UL>\n\
<LI>Caches for directory entries and ZIP file content. See "); SF_SRC_H(ZIPsFS_configuration); C("</LI>\n\
<LI>Removing files from page cache after usage</LI>\n\
</UL></LI>\n</UL>\n");
    SF_REF(HOMEPAGE);
    break;
    CASE_ID3(PRELOAD_SELECT_ALL,PRELOAD_SELECT_REMOTE,PRELOAD_SELECT_ZIP,"Selectors for preloading");
    C("<UL>\n");
    C("<LI>Subfolder ");SF_VALUE("/a/");C(" All files</LI>");
    C("<LI>Subfolder ");SF_VALUE("/r/");C(" Remote files</LI>");
    C("<LI>Subfolder ");SF_VALUE("/z/");C(" Files that are ZIP items</LI>");
    C("</UL>");
    break;
#define L(d) C("Files will be copied into "ANSI_FG_BLUE);C(_mnt_apparent); C(_writable_path);C(d ANSI_RESET".\n")
#define HEADLINE_PRELOAD() C("<H1>Preloading files improves efficiency of file reading</H1>\nZIPsFS can cache file content on the (l) local disk  or in the (m) RAM.\nThis  improves non-sequential  reading from a file.<BR>\n");

    CASE_ID2(PRELOADDISK_ENTIRE_ZIP,PRELOADDISK_ENTIRE_ZIP_NOT,"Preloading the entire ZIP");
    C("For remote paths, entire ZIP files will be cached on local disk.\n\
Consider the following two example files that are available when ZIPsFS is started with "); SF_SRC_SH(ZIPsFS_testfiles_start_ZIPsFS);C(".<BR><BR>\n\
<UL><LI><TT>head  mnt/db/pride/2022/12/PXD036786/20220121_Z1_ZW_001_30-0043_K562_SWATH_3.91ng_3.wiff2</TT></LI>\
<LI><TT>head mnt/db/pride/2026/07/PXD062492/20231031_PRO3_KTT_174_50-0121_AstraZeneca-PROTAC_P07_B09.d/analysis.tdf</TT></LI></UL>\n\
Both files originate from a FUSE mounted FTP site.\n\
Extraction of large ZIP entries with rclone, AVFS and curlftpfs is problematic.\n\
As a workaround, ZIPsFS will fetch the entire ZIP file with <TT>/usr/bin/curl</TT>.<BR><BR>\n\
Whether ZIPsFS internally saves the ZIP entry as a file or not depends on whether preloading into RAM is advised in ");SF_SRC_CH(ZIPsFS_configuration);C(".\n\
Only in the first example, the ZIP entry is extracted and cached as a separate file.\n\
With the second example, files with the ending .tdf are  preloaded into RAM.");
    break;
    CASE_ID3(INTERNET_UPDATE,PRELOAD_UPDATE,INTERNET_HEADER,"Updating the local copy of remote or compressed files");
    C("A HTML report is generated for each pre-loaded file found on the local HD.\
The paths of the pre-loaded file and the original source will displayed.\n\
Reading the HTML files in this folder, triggers the update process of the corresponding file.\n");
    C(id==ID_(INTERNET_UPDATE)?"The HTTP or FTP header data will be evaluated.\n":"The last-modified file attributes of the remote and local files will be compared.\n");
    C("If the local copy is out-of-date, it will be refreshed.\n\
On success, the copy on the local disk will be replaced. Success or failure will be reported in HTML.\n");
    break;
    CASE_ID1(PRELOADRAM,"Preloading to RAM");SF_REQUIRES(WITH_PRELOADDISK);
    C("Preloading to RAM is fast and does not use the local  disk. It is not good for files with long transfer times which are used more than once.\n\
PRELOADRAM can be activated  by file name based rules in "); SF_SRC_C(ZIPsFS_configuration);C(". The flags in the folder name take precedence.\n");
    break;
    CASE_ID2(PRELOADDISK_NOT,PRELOADDISK,"Preload files to local disk");SF_REQUIRES(WITH_PRELOADDISK);
    C("Caching on local disk is preferred over RAM in case of long transfer times.  File  used multiple times are transferred only once.\n");
    SF_REQUIRES_W();
    C("<H2>Mark entire file branch</H2>\nIn the command line to start  ZIPsFS, root paths can be followed by the property "); C(ROOT_PROPERTY[ROOT_PROPERTY_preload]);
    SF_SYMBOL_B();
    FOR(iCompress,COMPRESSION_NIL+1,COMPRESSION_NUM){ C(" &nbsp;  @"); C(ROOT_PROPERTY[ROOT_PROPERTY_preload]);C(cg_compression_file_ext(iCompress,NULL));}
    SF_SYMBOL_E();
    SF_BR();
    sf_update(b);
    sf_cleanup(b);
#undef D
#undef L
    break;
    CASE_ID1(VIEWMOD_DECOMPRESS,"Decompression of compressed files");SF_REQUIRES(WITH_PRELOADDISK);
    C("For all upstream compressed (bz2, gz, lrz, xz) files, decompressed virtual files will be presented.\n\
Upon first usage they will be downloaded. Once downloaded, the file sizes will be correctly displayed.");
    break;
    CASE_ID1(VIEWMOD_KEEP_ZIP,"Not expanding ZIP files");
    C("This directory provides  access to all files without expanding ZIP files.<BR>This also accelerates navigation and searching.\n\n");
    break;
    CASE_ID2(VIEWMOD_1,VIEWMOD_1_NOT,"The first file branch");
    C("The first root is writable and contains modified, generated or downloaded files. This file tree contains only those files that reside ");
    if (id==ID_(VIEWMOD_1_NOT)){ SF_SYMBOL_B();C("NOT ");SF_SYMBOL_E();}
    C("in the first root.\n");
    SF_REQUIRES_W();
    break;
    //triggers
  case SF_README_FOR_DIR(DIR_LOGGED):
    C("File access is logged in file "); C(_specialfiles[SFILE_LOG_FUNCTION_CALLS].rp);	C(".\n\
The logs can be used to identify misbehaving software which should rather be used with preloading for remote or compressed files.\n\
  - Excessive requests of file attributes\n\
  - Multiple open/close\n\
  - Backward seek\n\
  - Upper/lower case conversion of file names\n\n\
Run "); C(_specialfiles[SFILE_LOGGING_COMMAND].name);C(" to watch or view the logs.\n");
    break;
  case SFILE_LOGGING_COMMAND:{
    static const char *awk=
#include "tmp/include_print_tsv.awk.c"
      shebang_bash(false,b);
    C("f=");C(_specialfiles[SFILE_LOG_FUNCTION_CALLS].rp);
    C("\n[[ ! -f $f ]] && echo 'Not a file '$f && f=${0%/*}/");C(_specialfiles[SFILE_LOG_FUNCTION_CALLS].name);
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

    CASE_ID1(VIEWMOD_INTERNET,"Accessing files from the internet");SF_REQUIRES(WITH_INTERNET_DOWNLOAD);
    SF_REQUIRES_W();
    C("The directory "); SF_REF(DIR_INTERNET);
    C("provides access to internet files.\n\
The file names are formed from URLs by replacing the colon and slashes by comma.\n\
The time attributes are taken from the http or ftp headers. The headers are stored in separate files.\n\
The utility curl must be installed on the computer.\n\n\
<U>Examples:</U><UL>\n\
<LI>");SF_REF_DIR_NET("ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE");C("</LI>\n\
<LI>");SF_REF_DIR_NET("https,,,ftp.uniprot.org,pub,databases,uniprot,README")C("</LI>\n\
<LI>");SF_REF_DIR_NET("https,,,files.rcsb.org,download,1SBT.pdb");C("</LI>\n\
<LI>");SF_REF_DIR_NET("ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml");C("</LI>\n\
<LI>");SF_REF_DIR_NET("ftp,,,ftp.uniprot.org,pub,databases,uniprot,previous_releases,release-2022_05,reference_proteomes,Eukaryota,UP000005640,UP000005640_9606_canonical.fasta");
    C("</LI>\n</UL>\n\
In the last case, the file is only available as a gz compressed file. When the '<TT>.gz</TT>' suffix is omitted, the file will be decompressed during data transfer.\n\
Before, the files size is unknown and estimated.\n\
To update already downloaded files see folder"); SF_VALUE(DIRNAME_INTERNET_UPDATE);
    C("\n<BR><BR>The function config_internet_hardlink_filename() may return an additional file path where the downloaded file is copied to as well.\n");
    char additional_link[PATH_MAX];config_internet_hardlink_filename(NULL,additional_link,PATH_MAX,NULL,0);
    C("The current file location is "); SF_VALUE(additional_link);

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
    CASE_ID1(VIEWMOD_NIL,"No operation");
    C("Subfolder ");SF_VALUE("/-/");C(" provides default view on the file tree. ");
    break;
    CASE_ID1(VIEWMOD_FILECONVERSION,"File-conversion - Files generated automatically from other files");SF_REQUIRES(WITH_FILECONVERSION);
    SF_REQUIRES_W();
    C("<BR>The file tree ");
    sf_ref_special_dir(b,ID_(VIEWMOD_FILECONVERSION));
    C("contains all files along with dynamically generated files. The files come into existence only when used.\n\
They will be created using commands specified in ");
    SF_SRC_C(ZIPsFS_configuration_fileconversion);
    C(".<BR>\n\
<H2>Initiate file generation</H2>\n\
To force file generation, it is sufficient to read at least one byte of the file.  UNIX commands <TT>head</TT> can be used.\n\
For Windows, the  script "); sf_ref_special_dir(b,SFILE_READ_BEGINNING_OF_FILES_BAT); C(" is convenient.<BR>\n\
<H2>Unknown file size</H2>\n\
As long as the file content has not been  generated, the reported file size is incorrect which might be a problem for some software.\n");
    sf_cleanup(b);
    C("Also See "); SF_SRC_C(ZIPsFS_configuration_c);
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
  if (isHTML) SF_HTML_END();
}

#undef C
#undef SF_REQUIRES
#undef SF_PRINTF
