/************************************************************************/
/* COMPILE_MAIN=ZIPsFS                                                  */
/* Special generated files                                              */
/************************************************************************/
_Static_assert(WITH_PRELOADRAM,"");

#define C(txt) textbuffer_add_segment(TXTBUFSGMT_NO_FREE,b,txt,0)
#define SF_HTML_HEADER()     C("<!DOCTYPE html><HTML><HEAD><TITLE>ZIPsFS</TITLE></HEAD><BODY>\n")
#define SF_HTML_END()        C("\n</BODY>\n</HTML>\n")
#define SF_BR()              C("<BR>\n")
#define SF_SYMBOL_B()        C("<TT><FONT color=\"#FF00FF\">")
#define SF_SYMBOL_E()        C("</FONT></TT>")
#define SF_SYMBOL(name)      SF_SYMBOL_B(),C(name),SF_SYMBOL_E()

#define SF_VALUE(name)       C("<TT><FONT color=\"blue\">"),C(name),C("</FONT></TT>")
#define SF_PRINTF(...)  snprintf(tmp,sizeof(tmp)-1,__VA_ARGS__),textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0)
#define SF_FILE_REF(fn) sf_file_ref(b,depth,fn)
#define SF_FILE_ITEM(fn,after) sf_file_item(b,depth,fn,after)




static void  _sf_src_hc(textbuffer_t *b, const enum enum_configuration_src  id,const char *hc){
  SF_SYMBOL_B();
  C(ZIPSFS_CONFIGURATION_S[id]);
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
  if (_root_writable) SF_VALUE(_root_writable_path); else C("None given at command line");
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

static bool special_file_set_stat(struct stat *st, const virtualpath_t *vipa){
  const int i=vipa->special_file_id;
  bool ok=false;
  if (i && i<SFILE_WITH_REALPATH_NUM){
    ok=!lstat(SFILE_REAL_PATHS[i],st);
  }else if (i>SFILE_BEGIN_IN_RAM){
    stat_init(st,special_file_size(i),NULL);
    time(&st->st_mtime);
    st->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
    st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
    ok=true;
  }else  if ((vipa->flags&ZP_IS_PATHINFO)){
    stat_init(st,PATH_MAX,NULL);
    ok=true;
  }else if (trigger_files(false,vipa->vp,vipa->vp_l)){
    stat_init(st,0,NULL);
    return true;
  }
  if (ok && ENDSWITH(vipa->vp,vipa->vp_l,".command")) st->st_mode|=(S_IXOTH|S_IXUSR|S_IXGRP);
  return ok;
}

static uint64_t special_file_file_content_to_fhandle(zpath_t *zpath,const int special_file_id){
  uint64_t fh=0;
  if (special_file_id || ZPF(ZP_IS_PATHINFO)){
  fHandle_t *d=fhandle_create(FHANDLE_SPECIAL_FILE,&fh,zpath);
  textbuffer_t *b=textbuffer_new(COUNT_MALLOC_PRELOADRAM_TXTBUF);
  cg_thread_assert_not_locked(mutex_fhandle);
  if (ZPF(ZP_IS_PATHINFO)){
    find_realpath_any_root(0,zpath,NULL);
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
  if (fd<0 || textbuffer_differs_from_filecontent_fd(&b,fd)){
    log_msg("Going to write %s ...\n",path);
    textbuffer_write_file(&b,path,0770);
  }else{
    log_msg(ANSI_FG_GREEN"Up-to-date %s\n"ANSI_RESET,path);
  }
  if (fd>0) close(fd);
  textbuffer_destroy(&b);
}

//////////////////////////////////////////////////////
/// Generate virtual files with immutable content. ///
//////////////////////////////////////////////////////
#define SHEBANG "#!/usr/bin/env bash\nset -u\n"
#define I "echo 'With this script, the last last-access time of r/w files in ZIPsFS can be changed.'\n\
echo 'Normally, autogenerated files are deleted " FILECONVERSION_DELETE_FILES_AFTER_DAYS " days after their last use.'\n\
echo 'If no command line arguments are given, the script will process the files in the clipboard.'\n"

#define P "echo '';echo 'Going to set last-access time for the selected files.'\n\
    echo 'Enter a number of hours to be added to current time.'\n\
    echo 'Type 0 to set last-access-time to the current time. Files will be deleted after " FILECONVERSION_DELETE_FILES_AFTER_DAYS " days.'\n\
    echo 'Type a negative number of hours to pretend, that files were accessed in the past. Files will be deleted earlier.'\n\
    echo 'Type a positive number to pretend that files were last accessed in the future. This will extend the life span of the files'\n"
#define B "\n  echo 'No files given. Please select files in the file browser.'\n"

static void special_file_content(textbuffer_t *b,const enum enum_special_files id){
  char tmp[333];
  if (id==SFILE_CLEAR_CACHE || id==SFILE_DEBUG_CTRL){
    SF_PRINTF(SHEBANG"my_stat(){\necho\n\
    local f=$1  p=${2:-0}\n\
    set -x;stat --format %%s %s/${f}_${p}_%s 2>/dev/null;set +x\n\
    echo;\n}\n",_mnt_apparent,ctrl_file_end());
  }

  const int depth=cg_count_chr(SFILE_PARENTS[id],'/');
  switch(id){
  case SFILE_DEBUG_CTRL:
    C("\naskWhichThread(){\n");
    FOR(t,1,PTHREAD_LEN){
      SF_PRINTF("  echo '  %d %s' >&2\n",t,PTHREAD_S[t]);
    }
    C("  local t=0\n\
read -r -p 'What thread?' -n 1 t\n\
[[ $t != [0-9] ]] && t=0\n\
echo  $t\n}\n\n");

#define A(act,txt) SF_PRINTF("echo '   %d  %s'\n",act,txt)
#define H(txt)     SF_PRINTF("echo '""%s"ANSI_RESET"'\n",txt)

#define L(what) "Trigger error due to "what" lock."
    //SF_REQUIRES(WITH_ASSERT_LOCK,"WITH_ASSERT_LOCK")"."
    H("Terminate");
    A(ACT_KILL_ZIPSFS,"Kill-ZIPsFS and print status");
    H("Blocked threads");
    A(ACT_FORCE_UNBLOCK,"Unblock thread even if blocked thread cannot be killed - not recommended.");
    A(ACT_CANCEL_THREAD,"Interrupt-thread.  ZIPsFS will restart the thread eventually");
    H("Pthread - Locks");
    A(ACT_NO_LOCK,L("missing"));
    A(ACT_BAD_LOCK,L("inappropriate"));
#undef L
#undef A
#undef H
    C("thread=0\nread -r -n 1 -p 'Choice? ' c\necho\nif [[ $c == [1-9] ]];then\n");
    SF_PRINTF("  [[ $c == %d ]] && thread=$(askWhichThread)\n",ACT_CANCEL_THREAD);
    C("  my_stat $c $thread\nfi");
    break;
  case SFILE_CLEAR_CACHE:
#define A(c) "echo '  "STRINGIZE(c)"    "#c "'\n"
    C( A(CLEAR_ALL_CACHES) A(CLEAR_DIRCACHE) A(CLEAR_ZIPINLINE_CACHE) A(CLEAR_STATCACHE) "\ncache='';[[ $# == 0 ]] && read -r -p 'What cache?' -n 1 cache\n");
    SF_PRINTF("for c in \"${@}\" $cache; do [[ $c == [0-9] ]] && my_stat %d $c; done\n",ACT_CLEAR_CACHE);
#undef A
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
#define L(d) C("Files will be copied into "ANSI_FG_BLUE);C(_mnt_apparent); C(_root_writable_path);C(d ANSI_RESET".\n")
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
    C("<H2>Mark entire file branch</H2>\nAt the command line, root paths can be followed by the property "); SF_SYMBOL("@"ROOT_PROPERTY_PRELOAD); C(".\n\
This is useful for remote sites. Furthermore the properties the following properties can indicate to decompress files that have a compression suffix:\n");
    SF_SYMBOL_B();
    FOR(iCompress,COMPRESSION_NIL+1,COMPRESSION_NUM){ C(" &nbsp;  @"ROOT_PROPERTY_PRELOAD);C(cg_compression_file_ext(iCompress,NULL));}
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
  case SFILE_README_PLAIN:
    C("This directory provides plain access to all files.\n\
      - not expanding ZIP files.\n\n\n\
Useful for rapid navigation and searching.\n\n");
    break;
  case SFILE_README_FIRST_ROOT:
    C("This directory contains only those files that reside in the first branch which is the writable branch.\n\
It may be used to selectively process files that are generated or downloaded from remote locations.\n");
    break;
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
#if WITH_FILECONVERSION
  case SFILE_SET_ATIME_SH:
    C(SHEBANG);
    C("ff=\"$*\"\n\
[[ $# == 0 ]] && f=\"$(xclip -o)\"\n\
no_comput=1 h=0 silent=0\n\
[[ ${1:-} == -i ]] && silent=1 && shift\n" I "\n\
[[ -z $ff ]] && ! ff=$(xclip -selection clipboard -o) && ff=$(pbpaste)\n\
if ! ((silent));then\n\
    echo 'Note use option -i to suppress interactive input.'\n" P "\n\
    read -r -e -i 0 -p 'Hours: ' h\n\
    h=${h//[^0-9-]/}\n\
    [[ -z $h ]] && h=0\n\
    read -r -p 'Create non-existing files? [y/N] '\n\
    [[ ${REPLY,,} == y* ]] && no_comput=0\n\
fi\n\
for f in $ff;do\n\
    echo \""ANSI_FG_BLUE"\"Processing $f ...\""ANSI_RESET"\"\n\
    ! ((no_comput)) && { head \"$f\"|head -n 3|strings; cat $f.log; }\n\
    ls \"$f\""MAGIC_SFX_SET_ATIME"$h\n\
    done\n");
    break;
  case SFILE_SET_ATIME_BAT: C("CLS\npowershell %~dp0\\%~n0.ps1 %*\n@pause\n"); break;

  case SFILE_READ_BEGINNING_OF_FILES_BAT:
    C("CLS\n\
@echo A problem of generated files in ZIPsFS is that file sizes might not be known before file  generation.\n\
@echo For unknown file sizes, an upper estimate is reported.\n\
@echo This is true for converted files and compressed preloaded files.\n\
@echo.\n@echo This script forces file generation by reading the beginning of all files in  the clipboard.\n\
@echo.\n@echo Please select files in Windows Explorer and then press Ctrl-C.\n\
@pause\n\
powershell -NoProfile -ExecutionPolicy Bypass -c\
\"get-clipboard -format FileDropList| Get-ChildItem -File | ForEach-Object { Write-Host '=== '$_.FullName' ===' -ForegroundColor Cyan; Get-Content $_ -Encoding String -TotalCount 3 }\"\n\
@pause\n");
    break;
  case SFILE_SET_ATIME_PS:
    C("\n\
$ff=$args\n\
if (!$ff -or !$ff.Length){ $ff=$(Get-Clipboard -format filedroplist);}\n\
$no_comput=1; $h=0\n"I"\
if ($ff){\n" P "\n\
    $h=$(read-host -Prompt 'Hours')\n\
    $no_comput=$Host.UI.PromptForChoice('Not yet existing files','Generate files that do not exist yet?', @('&Yes'; '&No'), 1)\n\
    }else{" B "}\n\
 " P "\n\
foreach($f in $ff){\n\
    Write-Host -ForegroundColor green -BackgroundColor white \"Processing $f ...\"\n\
    if (!$no_comput){\n\
        $r=New-Object System.IO.StreamReader -Arg \"$f\"\n\
        $r.ReadLine() |Format-Hex\n\
        $r.close()\n\
        gc \"$f.log\" 2>$NULL\n\
        }\n\
   ls $(-join($f,'"MAGIC_SFX_SET_ATIME"',(0+$($h -replace '[^0-9-+]')))) 2>$NULL;\n\
}\n\
read-host -Prompt 'Press Enter'\n");
    break;
#endif //WITH_FILECONVERSION
  case SFILE_INFO:
    LOCK_N(mutex_special_file, make_info(MAKE_INFO_HTML|MAKE_INFO_ALL));
    if (_info) textbuffer_add_segment(TXTBUFSGMT_NO_FREE,b,_info,_info_capacity);
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
