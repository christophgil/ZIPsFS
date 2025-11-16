///////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                 ///
/// Special generated files                                             ///
///////////////////////////////////////////////////////////////////////////
////////////////////////////////
/// Special Files           ///
//////////////////////////////

#define SGMT_MNT() textbuffer_add_segment(TXTBUFSGMT_NO_FREE,b,_mnt,0)
#define isSpecialFileInDir(i,path,path_l,dir) _isSpecialFileInDir(i,path,path_l,dir,sizeof(dir)-1)
static bool _isSpecialFileInDir(const enum enum_special_files i, const char *path,const int path_l, const char *dir, const int dir_l){
  const char *s=SPECIAL_FILES[i];
  return
    s && *s && path &&
    (dir_l!=DIR_ZIPsFS_L || !(SPECIAL_FILES_FLAGS[i]&SFILE_FLAG_OTHER_DIR)) &&
    path_l==dir_l+1+SPECIAL_FILES_L[i] &&
    path[dir_l]=='/' &&
    !strncmp(path,dir,dir_l) &&
    !strcmp(path+dir_l+1,s);
}
static int special_file_id(const char *vp,const int vp_l){
  IF1(WITH_INTERNET_DOWNLOAD, if (isSpecialFileInDir(SFILE_INTERNET_README, vp,vp_l,DIR_INTERNET)) return SFILE_INTERNET_README);
  IF1(WITH_AUTOGEN,           if (isSpecialFileInDir(SFILE_AUTOGEN_README,  vp,vp_l,DIR_AUTOGEN)) return SFILE_AUTOGEN_README);

  if (IS_IN_DIR_ZIPsFS(vp,vp_l)){
    FOR(i,1,SFILE_NUM)  if (isSpecialFileInDir(i,vp,vp_l,DIR_ZIPsFS)) return i;
  }
  return 0;
}
static bool is_special_file_memcache(const char *path,const int path_l){
  if (PATH_IS_FILE_INFO(path,path_l)) return true;
  const int id=special_file_id(path,path_l);
  return id && !(SFILE_FLAG_REAL&SPECIAL_FILES_FLAGS[id]);
}

static bool find_realpath_special_file(struct zippath *zpath){
  FOR(i,0,2){
    if (isSpecialFileInDir(i?SFILE_LOG_ERRORS:SFILE_LOG_WARNINGS,VP(),VP_L(),DIR_ZIPsFS)){
      zpath->realpath=zpath_newstr(zpath);
      zpath_strcat(zpath,_fWarningPath[i]);
      ZPATH_COMMIT_HASH(zpath,realpath);
      if (zpath_stat(zpath,NULL)){
        zpath->stat_vp.st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
        return true;
      }
    }
  }
  zpath_reset_keep_VP(zpath);
  return false;
}

static off_t special_file_length(const int i){
  static int _special_file_length[SFILE_NUM];
  lock(mutex_special_file);
  assert(i>0); assert(i<SFILE_NUM);
  if (!_special_file_length[i]){
    struct textbuffer b={0};
    special_file_content(&b,i);
    _special_file_length[i]=textbuffer_length(&b);
    //log_debug_now(" SPECIAL_FILES: %s len: %d",SPECIAL_FILES[i], _special_file_length[i]);
    textbuffer_destroy(&b);
    if (!_special_file_length[i]) _special_file_length[i]=-1;
  }
  unlock(mutex_special_file);
  return _special_file_length[i];
}

// SFILE_INFO
static bool special_file_set_statbuf(struct stat *stbuf,const char *path,const int path_l){
  bool ok=false;
  {
    const int i=special_file_id(path,path_l);
    if (i && !(SPECIAL_FILES_FLAGS[i]&SFILE_FLAG_REAL)){
      stat_init(stbuf,special_file_length(i),NULL);
      time(&stbuf->st_mtime);
      stbuf->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
      stbuf->st_ino=inode_from_virtualpath(path,path_l);
      ok=true;
    }
  }
  if (!ok && PATH_IS_FILE_INFO(path,path_l)){
    stat_init(stbuf, PATH_MAX,0);//_info_capacity?_info_capacity:0xFFFF,0);
    ok=true;
  }
  if (!ok && trigger_files(false,path,path_l)){
    stat_init(stbuf,0,NULL);
    return true;
  };

  if (ok && ENDSWITH(path,path_l,".command")) stbuf->st_mode|=(S_IXOTH|S_IXUSR|S_IXGRP);
  return ok;
}


static void special_file_file_content_to_fhandle(struct fHandle *d){
  if (d->memcache && d->memcache->txtbuf || !(d->flags&FHANDLE_FLAG_SPECIAL_FILE)) return;
  const char *vp=D_VP(d);
  const int vp_l=D_VP_L(d), i=special_file_id(vp,vp_l);
  const bool isPathInfo=PATH_IS_FILE_INFO(vp,vp_l);
  //log_entered_function("%s i: %d isPathInfo: %d",vp,i,isPathInfo);
  if (i>0 || isPathInfo){
    struct textbuffer *b=textbuffer_new(COUNT_MALLOC_MEMCACHE_TXTBUF);
    cg_thread_assert_not_locked(mutex_fhandle);
    if (isPathInfo){
      char withoutSfx[vp_l+1];
      stpcpy(withoutSfx,vp)[-(sizeof(VFILE_SFX_INFO)-1)]=0;
      static struct zippath zp;
      zpath_init(&zp,withoutSfx);
      if (!find_realpath_any_root(0,&zp,NULL)) return;
      char buf[zp.realpath_l+zp.entry_path_l+3];
      textbuffer_add_segment(TXTBUFSGMT_DUP,b,buf,sprintf(buf,"%s%s%s\n",zp.strgs+zp.realpath,zp.entry_path_l?"\t":"",zp.strgs+zp.entry_path));
    }else{
      special_file_content(b,i);
    }
    {
      lock(mutex_fhandle);
      if (!fhandle_set_text(d,b)){
        FREE_NULL_MALLOC_ID(b);
      }else{
        memcache_set_status(d,memcache_done);
        d->flags|=FHANDLE_FLAG_MEMCACHE_COMPLETE;
        //log_debug_now("%s isPathInfo: %d textbuffer_length: %ld",D_VP(d), isPathInfo, textbuffer_length(b));
      }
      unlock(mutex_fhandle);
    }
  }
}



static void special_file_content_to_file(const int id, const char *path){
  struct textbuffer b={0};
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



/////////////////////////////////////////////////////////////////////////////////////////
/// Generate comprehensive HTML report about the inner state of this FUSE file system ///
/////////////////////////////////////////////////////////////////////////////////////////
static bool make_info(const int flags){
  cg_thread_assert_locked(mutex_special_file);
  int l;
  while(true){
    l=print_all_info(flags);
    //log_verbose("_info_capacity: %d l: %d",_info_capacity,l);
    if (_info_capacity>l) break;
    char *info=_info;
    if (!(_info=realloc(_info,_info_capacity=l+8888))){
      free_untracked(info);
      warning(WARN_MALLOC|WARN_FLAG_EXIT,SPECIAL_FILES[SFILE_INFO],"realloc failed.\n");
      return false;
    }
  }
  memset(_info+l,' ',_info_capacity-l);
  return true;
}

//////////////////////////////////////////////////////
/// Generate virtual files with immutable content. ///
//////////////////////////////////////////////////////
#define SHEBANG "#!/usr/bin/env bash\nset -u\n"
#define I "echo 'With this script, the last last-access time of r/w files in ZIPsFS can be changed.'\n\
echo 'Normally, autogenerated files are deleted " AUTOGEN_DELETE_FILES_AFTER_DAYS " days after their last use.'\n\
echo 'If no command line arguments are given, the script will process the files in the clipboard.'\n"

#define P "echo '';echo 'Going to set last-access time for the selected files.'\n\
    echo 'Enter a number of hours to be added to current time.'\n\
    echo 'Type 0 to set last-access-time to the current time. Files will be deleted after " AUTOGEN_DELETE_FILES_AFTER_DAYS " days.'\n\
    echo 'Type a negative number of hours to pretend, that files were accessed in the past. Files will be deleted earlier.'\n\
    echo 'Type a positive number to pretend that files were last accessed in the future. This will extend the life span of the files'\n"
#define B "\n  echo 'No files given. Please select files in the file browser.'\n"

static void special_file_content(struct textbuffer *b,const enum enum_special_files i){
  char tmp[333];
  if (i==SFILE_CLEAR_CACHE || i==SFILE_DEBUG_CTRL){
    sprintf(tmp,SHEBANG"my_stat(){\necho\n\
    local f=$1  p=${2:-0}\n\
    set -x;stat --format %%s %s/${f}_${p}_%s 2>/dev/null;set +x\n\
    echo;\n}\n",_mnt,ctrl_file_end());
    textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0);
  }
  switch(i){

  case SFILE_DEBUG_CTRL:
    textbuffer_add_segment_const(b,"\naskWhichThread(){\n");
    FOR(t,1,PTHREAD_LEN){
      sprintf(tmp,"  echo '  %d %s' >&2\n",t,PTHREAD_S[t]);
      textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0);
    }
    textbuffer_add_segment_const(b,"  local t=0\n\
read -r -p 'What thread?' -n 1 t\n\
[[ $t != [0-9] ]] && t=0\n\
echo  $t\n}\n\n");

#define A(act,txt) sprintf(tmp,"echo '   %d  %s'\n",act,txt); textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0)
#define H(txt) sprintf(tmp,"echo '""%s"ANSI_RESET"'\n",txt); textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0)
#define L(what) "Trigger error due to "what" lock. Requires WITH_ASSERT_LOCK set to '1'.  Current value: '"STRINGIZE(WITH_ASSERT_LOCK)"'."
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
    textbuffer_add_segment_const(b,"thread=0\nread -r -n 1 -p 'Choice? ' c\necho\nif [[ $c == [1-9] ]];then\n");
    sprintf(tmp,"  [[ $c == %d ]] && thread=$(askWhichThread)\n",ACT_CANCEL_THREAD);
    textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0);
    textbuffer_add_segment_const(b,"  my_stat $c $thread\nfi");
    break;
  case SFILE_CLEAR_CACHE:
#define A(c) "echo '  "STRINGIZE(c)"    "#c "'\n"
    textbuffer_add_segment_const(b, A(CLEAR_ALL_CACHES) A(CLEAR_DIRCACHE) A(CLEAR_ZIPINLINE_CACHE) A(CLEAR_STATCACHE) "\ncache='';[[ $# == 0 ]] && read -r -p 'What cache?' -n 1 cache\n");
    sprintf(tmp,"for c in \"${@}\" $cache; do [[ $c == [0-9] ]] && my_stat %d $c; done\n",ACT_CLEAR_CACHE);
    textbuffer_add_segment(TXTBUFSGMT_DUP,b,tmp,0);
#undef A
    break;
  case SFILE_README:
    textbuffer_add_segment_const(b,"<!DOCTYPE html><HTML><HEAD><TITLE>ZIPsFS</TITLE></HEAD><BODY>\n<H1>The ZIPsFS FUSE file system.</H1><H2>Introduction</H2>\n\
This is the virtual file system ZIPsFS.<BR><BR>\n\
It can expand ZIP files and combine two or more file trees.<BR>\n\
Records consisting of several files and archived as a ZIP  file can be unfolded and appear as if they would not have been zipped.\n\
Software can directly work on the archived data without prior un-zipping.<BR><BR>\n\
The files and folders appear to be writable even if the primary storage is read-only.\n\
This is necessary for  software that uses wrong flags for opening files for reading or which creates output files in the same file location.<BR>\n\
<A href=\"" HOMEPAGE "\">Homepage</A><BR>\n\
<H2>Auto-generated files</H2>\n\
This feature can be activated with the switch <b>WITH_AUTOGEN</b> in ZIPsFS_configuration.h. It is currently <B>" IF0(WITH_AUTOGEN,"de")"activated</B>.\n\
Derived files are displayed in the file tree <B>");
    SGMT_MNT();
#if WITH_AUTOGEN
    textbuffer_add_segment_const(b, DIR_ZIPsFS"/"DIRNAME_AUTOGEN"</B>.\n\
With <b>WITH_AUTOGEN_DIR_HIDDEN</b> set to <b>1</b>, the folder <B>"DIRNAME_AUTOGEN"</B> is not listed in its parent.\n\
The prevents recursive file searches to enter this tree.\n\
It is currently <B>" IF0(WITH_AUTOGEN_DIR_HIDDEN,"de")"activated</B>.<BR><BR>\n\
The generated files  are displayed even if they do  not exist.\n\
They are generated when used for the first time.\n\
On subsequent usage, the files are available without delay.\n\
The generated files are disposed when they are not used within a customizeable number of days.\n\
This can be prevented by updating the last-access-time with the scripts<B>");
    SGMT_MNT();
    textbuffer_add_segment_const(b,DIR_ZIPsFS"/"FILENAME_SET_ATIME".*</B><BR><BR><BR>\n\
When a file is accessed that is not yet generated, potentially two  problems may occur:<OL>\n\
<LI>Since the file will be generated upon first usage, the reading software receives the file data with delay. It may be possible, that the respective software may not  cope with this delay. At system level, the call to  open() returns immediately, while, the first call to read() may take long.</LI>\n\
<LI>While a file does not exist yet, its file size is guessed. The  estimate needs to be  equal-or-larger than the unknown file size. Programs may have a propblem with inaccurate file sizes.</LI>\n\
</OL><B><U>Workaround</U></B>\n\
Generation of the files can be forced using "FILENAME_SET_ATIME". These scripts change the <B>last-access-time</B> and  can also be used to postpone  deletion.<BR><BR>\
<B><U>Testing</U></B>\n\
For testing,  copy  jpeg, png or give files. Downscaled versions will be found in the file tree "DIR_ZIPsFS"/"DIRNAME_AUTOGEN"/.\n\
This requires installation of Imagemagick\n\
</BODY></HTML>\n");
#endif // WITH_AUTOGEN
    break;



#if WITH_INTERNET_DOWNLOAD
  case SFILE_INTERNET_README:
    textbuffer_add_segment_const(b,"This directory contains downloaded internet files.\n\
The file names are the URLs where the colon and slashes are replaced by comma.\n\
ZIPsFS automatically downloads and updates the files.\n\
The http or ftp header is stored in a separate file.\n\
The utility curl is required.\n\
Examples: \n");
    SGMT_MNT();textbuffer_add_segment_const(b,DIR_INTERNET"/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE\n");
    SGMT_MNT();textbuffer_add_segment_const(b,DIR_INTERNET"/https,,,ftp.uniprot.org,pub,databases,uniprot,README\n");
    textbuffer_add_segment_const(b,"The subfolder /gz/ allows decompression on download. Note the trailing .gz in the following example:\n\n");
    SGMT_MNT();textbuffer_add_segment_const(b,DIR_INTERNET"/gz/https,,,files.rcsb.org,download,1SBT.pdb\n");
    textbuffer_add_segment_const(b,"Here the file size will initially be wrong because the HTTP header is lacking the Content-Length field.\n");
    break;
#endif //WITH_INTERNET_DOWNLOAD
#if WITH_AUTOGEN
  case SFILE_AUTOGEN_README:
    textbuffer_add_segment_const(b,"This file tree is a replicate of "); SGMT_MNT();
    textbuffer_add_segment_const(b,".\nIn addition it contains dynamically generated files which might or might not exist as real files yet.\n\
They will be created on access using comand lines which can be customized by the user.\n\
Before file generation, the file size is an incorrect guess. Usually it is an overestimation.\n");
    break;
  case SFILE_SET_ATIME_SH:
    textbuffer_add_segment_const(b,SHEBANG);
    textbuffer_add_segment_const(b,"ff=\"$*\"\n\
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
  case SFILE_SET_ATIME_BAT: textbuffer_add_segment_const(b,"powershell %~dp0\\%~n0.ps1 %*\n@pause\n"); break;
  case SFILE_SET_ATIME_PS:
    textbuffer_add_segment_const(b,"\n\
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
#endif //WITH_AUTOGEN
  case SFILE_INFO:
    LOCK_N(mutex_special_file, make_info(MAKE_INFO_HTML|MAKE_INFO_ALL));
    if (_info) textbuffer_add_segment(TXTBUFSGMT_NO_FREE,b,_info,_info_capacity);
    break;
  default:;
  }
}
#undef T
#undef I
#undef P
#undef B
