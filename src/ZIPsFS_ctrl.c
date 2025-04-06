/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                     ///
/// Controling ZIPsFS by the user accessing magic file names  ///
/////////////////////////////////////////////////////////////////
// cppcheck-suppress-file literalWithCharPtrCompare
#define SHEBANG "#!/usr/bin/env bash\nset -u\n"
#define MAGIC_SFX_SET_ATIME  ".magicSfxSetAccessTime"
#define I "echo 'With this script, the last last-access time of r/w files in ZIPsFS can be changed.'\n\
echo 'Normally, autogenerated files are deleted " AUTOGEN_DELETE_FILES_AFTER_DAYS " days after their last use.'\n\
echo 'If no command line arguments are given, the script will process the files in the clipboard.'\n"

#define P "echo '';echo 'Going to set last-access time for the selected files.'\n\
    echo 'Enter a number of hours to be added to current time.'\n\
    echo 'Type 0 to set last-access-time to the current time. Files will be deleted after " AUTOGEN_DELETE_FILES_AFTER_DAYS " days.'\n\
    echo 'Type a negative number of hours to pretend, that files were accessed in the past. Files will be deleted earlier.'\n\
    echo 'Type a positive number to pretend that files were last accessed in the future. This will extend the life span of the files'\n"
#define B "\n  echo 'No files given. Please select files in the file browser.'\n"

enum _CTRL_ACTION{ACT_NIL,ACT_KILL_ZIPSFS,ACT_BLOCK_THREAD,/*ACT_UNBLOCK_THREAD,*/ACT_FORCE_UNBLOCK,ACT_CANCEL_THREAD,ACT_NO_LOCK,ACT_BAD_LOCK,ACT_CLEAR_CACHE};

static char *ctrl_file_end(){
  static char s[222]={0};
  if (!*s){
    struct timespec t;
    timespec_get(&t,TIME_UTC);
    srand(time(0));
    int r1=rand();
    srand(t.tv_nsec);
    int r2=rand();
    srand(getpid());
    sprintf(s,"%x%x%x%lx%lx",r1,r2,rand(),t.tv_nsec,t.tv_sec);
  }
  return s;
}

static void special_file_content(struct textbuffer *b,const enum enum_special_files i){
  char tmp[333];
  if (i==SFILE_CLEAR_CACHE || i==SFILE_DEBUG_CTRL){
    sprintf(tmp,SHEBANG"my_stat(){\necho\n\
    local f=$1  p=${2:-0}\n\
    set -x;stat --format %%s %s/${f}_${p}_%s 2>/dev/null;set +x\n\
    echo;\n}\n",_mnt,ctrl_file_end());
    textbuffer_add_segment(0,b,strdup_untracked(tmp),0);
  }
  switch(i){
  case SFILE_DEBUG_CTRL:
    textbuffer_add_segment_const(b,"\naskWhichThread(){\n");
    FOR(t,PTHREAD_NIL+1,PTHREAD_LEN){
      sprintf(tmp,"  echo '  %d %s' >&2\n",t,PTHREAD_S[t]);
      textbuffer_add_segment(0,b,strdup_untracked(tmp),0);
    }
    textbuffer_add_segment_const(b,"  local t=0\n\
read -r -p 'What thread?' -n 1 t\n\
[[ $t != [0-9] ]] && t=0\n\
echo  $t\n}\n\n");

#define A(act,txt) sprintf(tmp,"echo '   %d  %s'\n",act,txt); textbuffer_add_segment(0,b,strdup_untracked(tmp),0)
#define H(txt) sprintf(tmp,"echo '""%s"ANSI_RESET"'\n",txt); textbuffer_add_segment(0,b,strdup_untracked(tmp),0)
#define L(what) "Trigger error due to "what" lock. WITH_ASSERT_LOCK is " IF1(WITH_ASSERT_LOCK,"1. ZIPsFS will abort. ") IF0(WITH_ASSERT_LOCK,"0.  This will not work unless WITH_ASSERT_LOCK is set to 1 in ZIPsFS_configuration.h.")
    H("Terminate");
    A(ACT_KILL_ZIPSFS,"Kill-ZIPsFS and print status");
    H("Blocked threads");
    A(ACT_BLOCK_THREAD,"Simulate-blocking-thread. ZIPsFS will release the block eventually");
    //    A(ACT_UNBLOCK_THREAD,"Undo-simulate-blocking-thread");
    A(ACT_FORCE_UNBLOCK,"Unblock thread even if blocked thread cannot be killed - not recommended.");
    A(ACT_CANCEL_THREAD,"Interrupt-thread.  ZIPsFS will restart the thread eventually");
    H("Pthread - Locks");
    A(ACT_NO_LOCK,L("missing"));
    A(ACT_BAD_LOCK,L("inappropriate"));
#undef L
#undef A
#undef H
    textbuffer_add_segment_const(b,"thread=0\nread -r -n 1 -p 'Choice? ' c\necho\nif [[ $c == [1-9] ]];then\n");
    sprintf(tmp,"  [[ $c == %d  || $c == %d ]] && thread=$(askWhichThread)\n",ACT_BLOCK_THREAD,ACT_CANCEL_THREAD);
    textbuffer_add_segment(0,b,strdup_untracked(tmp),0);
    textbuffer_add_segment_const(b,"  my_stat $c $thread\nfi");
    break;
  case SFILE_CLEAR_CACHE:
#define A(c) "echo '  "STRINGIZE(c)"    "#c "'\n"
    textbuffer_add_segment_const(b, A(CLEAR_ALL_CACHES) A(CLEAR_DIRCACHE) A(CLEAR_ZIPINLINE_CACHE) A(CLEAR_STATCACHE) "\ncache='';[[ $# == 0 ]] && read -r -p 'What cache?' -n 1 cache\n");
    sprintf(tmp,"for c in \"${@}\" $cache; do [[ $c == [0-9] ]] && my_stat %d $c; done\n",ACT_CLEAR_CACHE);
    textbuffer_add_segment(0,b,strdup_untracked(tmp),0);
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
    textbuffer_add_segment(0,b,_mnt,0);
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
    textbuffer_add_segment(0,b,_mnt,0);
    textbuffer_add_segment_const(b,DIR_ZIPsFS FN_SET_ATIME".*</B><BR><BR><BR>\n\
When a file is accessed that is not yet generated, potentially two  problems may occur:<OL>\n\
<LI>Since the file will be generated upon first usage, the reading software receives the file data with delay. It may be possible, that the respective software may not  cope with this delay. At system level, the call to  open() returns immediately, while, the first call to read() may take long.</LI>\n\
<LI>While a file does not exist yet, its file size is guessed. The  estimate needs to be  equal-or-larger than the unknown file size. Programs may have a propblem with inaccurate file sizes.</LI>\n\
</OL><B><U>Workaround</U></B>\n\
Generation of the files can be forced using "FN_SET_ATIME". These scripts change the <B>last-access-time</B> and  can also be used to postpone  deletion.<BR><BR>\
<B><U>Testing</U></B>\n\
For testing,  copy  jpeg, png or give files. Downscaled versions will be found in the file tree "DIR_ZIPsFS"/"DIRNAME_AUTOGEN"/.\n\
This requires installation of Imagemagick\n\
</BODY></HTML>\n");
#endif // WITH_AUTOGEN
    break;
#if WITH_AUTOGEN
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
    textbuffer_add_segment(0,b,_info,_info_l);
  default:;
  }
}
#undef T
#undef I
#undef P
#undef B
static bool trigger_files(const bool isGenerated,const char *path,const int path_l){
  if (cg_endsWith(path,path_l,ctrl_file_end(),0)){
    int action=-1, para=-1;
    sscanf(path+cg_last_slash(path)+1,"%d_%d_",&action,&para);
    const int thread=PTHREAD_NIL<para && para<PTHREAD_LEN?para:0;
    warning(WARN_DEBUG,path,"Triggered action: %d  para: %d ",action,para);
    if (action>0){
      struct rootdata *one_root=_root;
      foreach_root1(r){
        if (r->features&ROOT_REMOTE) one_root=r;
        switch(action){
          //case ACT_UNBLOCK_THREAD:          memset(r->thread_pretend_blocked,0,sizeof(r->thread_pretend_blocked));          return true;
        case ACT_FORCE_UNBLOCK: _thread_unblock_ignore_existing_pid=true; break;
        case ACT_CANCEL_THREAD:
          if (thread && r->thread[thread]) pthread_cancel(r->thread[thread]);
          break;
        case ACT_CLEAR_CACHE:
          IF1(WITH_CLEAR_CACHE, if (0<=para && para<CLEAR_CACHE_LEN) dircache_clear_if_reached_limit_all(true,para?(1<<para):0xFFFF));
          return true;
        case ACT_KILL_ZIPSFS:
          LOCK(mutex_special_file,make_info(0);fputs(_info,stderr));
          IF1(WITH_PROFILER,print_profile());
          DIE("Killed due to  ACT_KILL_ZIPSFS");
          break;
        case ACT_BAD_LOCK:
        case ACT_NO_LOCK:
          IF0(WITH_ASSERT_LOCK, log_warn("WITH_ASSERT_LOCK is 0");return true);
#define p() log_strg("The following will pass ...\n")
#define f() log_strg("The following will trigger assertion ...\n")
#define e() log_error("The previous line should have triggered assertion.\n");
#if 1
#define G() ht_get(&r->dircache_ht,"key",3,0)
#else
#define G() cg_assert_lockedmutex_dircache)
#endif
          if (action==ACT_NO_LOCK){
            LOCK(mutex_dircache,p();G());
            log_strg(GREEN_SUCCESS"\n");
            f();
            G();
            e();
          }else{
            p();
            cg_thread_assert_not_locked(mutex_fhandle);
            log_strg(GREEN_SUCCESS"\n");
            f();
            LOCK(mutex_fhandle,cg_thread_assert_not_locked(mutex_fhandle));
            e();
          }
#undef p
#undef f
#undef e
#undef G
          return true;
        }/*switch*/
      }/* foreach_root*/
      if (action==ACT_BLOCK_THREAD && thread && one_root){
        warning(WARN_THREAD,rootpath(one_root),"Going to simulate block %s",PTHREAD_S[thread]);
        one_root->thread_pretend_blocked[thread]=true;
      }
      return true;
    }/*if(action)*/
  }
  char *posHours=strstr(path,MAGIC_SFX_SET_ATIME);
  if (posHours){
    const int len=(int)(posHours-path);
    posHours+=sizeof(MAGIC_SFX_SET_ATIME)-1;
    char path2[MAX_PATHLEN+1];
    cg_stpncpy0(path2,path,len);
    bool found;FIND_REALPATH(path2);
    if (found){
      cg_file_set_atime(RP(),&zpath->stat_rp,3600L*atoi(posHours));
      return true;
    }
  }/*posHours*/
  return false;
}/*trigger_files()*/
static int read_special_file(const int i, char *buf, const off_t size, const off_t offset){
  lock(mutex_special_file);
  struct textbuffer b={0};
  special_file_content(&b,i);
  const int l=textbuffer_length(&b), n=MIN_int(size,l-(int)offset);
  if (n>0 && buf)  textbuffer_copy_to(&b,offset,offset+n,buf);
  unlock(mutex_special_file);
  return n<0?EOF:n;
}

static void make_info(const int flags){
  cg_thread_assert_locked(mutex_special_file);
  int l;
  _info_capacity=9999;
  _info=malloc_untracked(_info_capacity);
  while((l=print_all_info(flags))>=_info_capacity){
    char * tmp=realloc(_info,_info_capacity=_info_capacity*2+0x100000+17);
    if (!tmp){ fprintf(stderr,"realloc failed.\n"); EXIT(1);};
    _info=tmp;
  }
  _info_l=l;
}
// https://www.vbforums.com/showthread.php?557980-RESOLVED-2008-Force-a-refresh-of-all-Win-Explorer-windows-showing-a-particular-folder
// https://ccms-ucsd.github.io/GNPSDocumentation/fileconversion/
