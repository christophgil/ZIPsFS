/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                     ///
/// Controling ZIPsFS by the user accessing magic file names  ///
/////////////////////////////////////////////////////////////////

#define SHEBANG "#!/usr/bin/env bash\nset -u\n"
#define MAGIC_SFX_SET_ATIME  ".magicSfxSetAccessTime"
#define F_CLEAR_CACHE     "/ZIPsFS_CLEAR+"
#define F_KILL_ZIPSFS     "/ZIPsFS_KILL+"
#define F_BLOCK_THREAD    "/ZIPsFS_BLOCK+"
#define F_UNBLOCK_THREAD  "/ZIPsFS_UNBLOCK+"
#define F_CANCEL_THREAD   "/ZIPsFS_CANCEL+"
#define F_NO_LOCK         "/ZIPsFS_NO_LOCK+"
#define F_BAD_LOCK        "/ZIPsFS_BAD_LOCK+"

static char *CTRL_FILES[]={F_CLEAR_CACHE,F_KILL_ZIPSFS,F_BLOCK_THREAD,F_UNBLOCK_THREAD,F_CANCEL_THREAD,F_BAD_LOCK,F_NO_LOCK,NULL};
static char BASH_SECRET[40]={0};
#define F(f,ask,p) "$" #f ") my_stat $" #f " " f " "  #ask " "  #p ";return;;\n"
#define C(clear)  #clear ") my_stat " #clear " $FC 0 " STRINGIZE(clear)  ";break;;\n"
#define SFILE_CTRL_TEXT_BEGIN "\n\
readonly CANCEL=Cancel\n\
dir=${BASH_SOURCE[0]}\n\
dir=${dir%/*}\n\
yes_no(){\n echo\n\
    read -p \"Are you sure to $1 [Y/n] ?\" -n 1 -r\n\
    [[ -z $REPLY || ${REPLY,,} == y ]] && return 0\n\
    echo Canceled;return 1\n\
}\n\
my_stat(){\n\
    local what=\"$1\" f=\"$2\" ask=${3:-0} p=${4:-0}\n\
    [[ $p == ~ ]] && p=$(askWhichThread)\n\
    ((ask)) && ! yes_no \"$what\" && return\n\
    set -x;stat \"$MNT/$f$VERS$p\" 2>/dev/null|grep -F 'File:';set +x\n\
    echo;echo \"$what  done.\"\n\
}\n\
debug_cases(){ :; }\n\
F_CANCEL_THREAD=''  F_BLOCK_THREAD='' F_KILL_ZIPSFS='' F_UNBLOCK_THREAD='' F_NO_LOCK='' F_BAD_LOCK=''\n"


#define SFILE_CTRL_TEXT_END "select s in $CANCEL CLEAR_ALL_CACHES CLEAR_DIRCACHE CLEAR_STATCACHE CLEAR_ZIPINLINE_CACHE $F_CANCEL_THREAD $F_BLOCK_THREAD $F_UNBLOCK_THREAD $F_NO_LOCK $F_BAD_LOCK $F_KILL_ZIPSFS;do\n\
FC="F_CLEAR_CACHE"\n\
case $s in\n$CANCEL) break;;\n" C(CLEAR_ALL_CACHES) C(CLEAR_DIRCACHE) C(CLEAR_STATCACHE) C(CLEAR_ZIPINLINE_CACHE) "\nesac\n\
  debug_cases $s && break\n\
done\n"

#define SFILE_CTRL_TEXT_OPTIONAL "\ndebug_cases(){\n\
case $1 in\n" F(F_CANCEL_THREAD,0,~) F(F_BLOCK_THREAD,1,~) F(F_UNBLOCK_THREAD,0,~) F(F_KILL_ZIPSFS,1,) F(F_NO_LOCK,1,0) F(F_BAD_LOCK,1,0) "\nesac\n\
return 1\n}\n\
F_CANCEL_THREAD=Interrupt-thread\n\
F_BLOCK_THREAD=Simulate-blocking-thread F_UNBLOCK_THREAD=Undo-simulate-blocking-thread\n\
F_NO_LOCK='Simulate-error-no-lock' F_BAD_LOCK='Simulate-error-bad-lock'\n\
F_KILL_ZIPSFS=Kill-ZIPsFS\n\
askWhichThread(){\n\
    cat >&2 <<EOF\n\
S  Stat Queue\n\
M  in Mememory Cache\n\
D  Cache of File Directory Entries\n\
R  File Root Respond\n\
EOF\n\
    read -p 'What thread [S/M/D/R]?' -n 1 -r\n\
    echo -n ${REPLY^^}\n\
}\n"

#define I "echo 'With this script, the last last-access time of r/w files in ZIPsFS can be changed.'\n\
echo 'Normally, autogenerated files are deleted " AUTOGEN_DELETE_FILES_AFTER_DAYS " days after their last use.'\n\
echo 'By updating the last-access file attribute, deletion can be postponed.'\n\
echo 'If no command line arguments are given, the script will process the files in the clipboard.'\n"

#define P "echo '';echo 'Going to set last-access time for the selected files.'\n\
    echo 'Enter a number of hours to be added to current time.'\n\
    echo 'Type 0 to set last-access-time to the current time. Files will be deleted after " AUTOGEN_DELETE_FILES_AFTER_DAYS " days.'\n\
    echo 'Type a negative number of hours to pretend, that files were accessed in the past. Files will be deleted earlier.'\n\
    echo 'Type a positive number to pretend that files were last accessed in the future. This will extend the life span of the files'\n"
#define B "\n  echo 'No files given. Please select files in the file browser.'\n"




static void special_file_content(struct textbuffer *b,const enum enum_special_files i){
  char *a=STRINGIZE(CLEAR_DIRCACHE);
  if (!*BASH_SECRET){
    struct timespec t;
    timespec_get(&t,TIME_UTC);
    sprintf(BASH_SECRET,"%d",(int)((t.tv_sec+t.tv_nsec)&0xFFFFFF));
  }
  switch(i){
  case SFILE_CTRL:   case SFILE_DEBUG_CTRL:
    textbuffer_add_segment_const(b,SHEBANG"readonly MNT=");textbuffer_add_segment(b,_mnt,0);
    textbuffer_add_segment_const(b,"\nreadonly VERS=");textbuffer_add_segment(b,BASH_SECRET,strlen(BASH_SECRET));
    textbuffer_add_segment_const(b,SFILE_CTRL_TEXT_BEGIN);
    if (i==SFILE_DEBUG_CTRL) textbuffer_add_segment_const(b,SFILE_CTRL_TEXT_OPTIONAL);
    textbuffer_add_segment_const(b,"\n"SFILE_CTRL_TEXT_END);
    break;
  case SFILE_README:
    textbuffer_add_segment_const(b,"<!DOCTYPE html><HTML><HEAD><TITLE>ZIPsFS</TITLE></HEAD><BODY>\n<H1>The ZIPsFS FUSE file system.</H1><H2>Introduction</H2>\n\
This is the virtual file system ZIPsFS.<BR><BR>\n\
It can expand ZIP files and combine two or more file trees.<BR>\n\
Records consisting of several files and archived as a ZIP  file can be unfolded and appear as if they would not have been zipped.\n\
Software can directly work on the archived data without prior un-zipping.<BR><BR>\n\
Write permission is pretended even for read-only file locations.\n\
This is necessary for  software that uses wrong flags for opening files for reading or which creates output files in the same file location.<BR>\n\
<A href=\"" HOMEPAGE "\">Homepage</A><BR>\n\
<H2>Auto-generated files</H2>\n\
This feature can be (de-)activated with the switch <b>WITH_AUTOGEN</b> in ZIPsFS_configuration.h. It is currently <B>" IF0(WITH_AUTOGEN,"de")"activated</B>.<BR>\n\
Derived files are displayed in the file tree even when they do not exist yet.\n\
They are generated when used for the first time.\n\
On subsequent usage, the files are available without delay.\n\
The generated files are disposed when they are not used within a customizeable number of days.\n\
This can be prevented by updating the last-access-time with the scripts <B>."FN_SET_ATIME".*</B><BR><BR><BR>\n\
When a file is accessed which is not yet generated potentially two  problems may occur:<OL>\n\
<LI>Since the file will be generated upon first usage, the reading software receives the file data with delay. It may be possible, that the respective software may not  cope with this delay. At system level, the call to  open() returns prompt, while, the first call to read() may take some time.</LI>\n\
<LI>While a file does not exist yet, its file size is guessed. The  estimate needs to be  equal-or-larger than the unknown file size. Programs may have a propblem with inaccurate file sizes.</LI>\n\
</OL><B><U>Workaround</U></B>\n\
Generation of the files can be forced using the scripts"FN_SET_ATIME". These scripts change the <B>last-access-time</B> and  can therefore also be used to prevent deletion after after timeout.<BR>\
</BODY></HTML>\n");
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
    echo 'Note use option -i to suppress interactive input.'\n\
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
  case SFILE_SET_ATIME_PS: textbuffer_add_segment_const(b,"\n\
$ff=$args\n\
if (!$ff -or !$ff.Length){ $ff=$(Get-Clipboard -format filedroplist);}\n\
$no_comput=1; $h=0\n"I"\
if ($ff){\n\
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
  case SFILE_INFO: textbuffer_add_segment(b,_info,_info_l);
  default:;
  }
}
#undef F
#undef C

#undef I
#undef P
#undef B
static bool trigger_files(const bool isGenerated,const char *path,const int path_l){
  if (path_l>4 && strstr(path,BASH_SECRET)){
    const int z=path[path_l-1], t=z=='S'?PTHREAD_STATQUEUE: z=='M'?PTHREAD_MEMCACHE: z=='D'?PTHREAD_DIRCACHE: z=='R'?PTHREAD_RESPONDING:-1;
    char *f=NULL;
    for(int j=0;CTRL_FILES[j];j++) if (!memcmp(path,CTRL_FILES[j],strlen(CTRL_FILES[j])-1)) f=CTRL_FILES[j];
    //if (strstr(path,"/ZIPsFS"))      log_entered_function("%s op=%c t=%d ",path,op?op:'0',t);
    if (f){
      warning(WARN_DEBUG,path,"Triggered '%s' %s",f,t>=0?PTHREAD_S[t]:"");
      foreach_root(i,r){
        if (f==F_BLOCK_THREAD){
          if (t>=0) r->debug_pretend_blocked[t]=true;
        }else if (f==F_UNBLOCK_THREAD){
          memset(r->debug_pretend_blocked,0,sizeof(r->debug_pretend_blocked));
          return true;
        } if (f==F_CANCEL_THREAD){
          if (t>=0) pthread_cancel(r->pthread[t]);
        }else if (f==F_CLEAR_CACHE){
          IF1(DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,dircache_clear_if_reached_limit_all(true,z=='0'?0xFFFF:(1<<(z-'0'))));
          return true;
        }else if (f==F_KILL_ZIPSFS){
          //    char *memleak=malloc(100);        fprintf(stderr,"Going to run fusermount\n");    execlp("/usr/bin/fusermount","fusermount","-u",_mnt,NULL);
          exit_ZIPsFS(0);
          return true;
        }else if (f==F_BAD_LOCK || f==F_NO_LOCK){
          if (!WITH_ASSERT_LOCK) log_warn0("WITH_ASSERT_LOCK is 0");
          else{
#define p() log_strg("The following will pass ...\n")
#define f() log_strg("The following will trigger assertion ...\n")
#define e() log_error0("The previous line should have triggered assertion.\n");
#if 1
#define G() ht_get(&r->dircache_ht,"key",3,0)
#else
#define G() cg_assert_lockedmutex_dircache)
#endif
            if (f==F_NO_LOCK){
              LOCK(mutex_dircache,p();G());
              log_strg(GREEN_SUCCESS"\n");
              f();
              G();
              e();
            }else{
              log_debug_now0("F_BAD_LOCK");
              p();
              cg_thread_assert_not_locked(mutex_fhdata);
              log_strg(GREEN_SUCCESS"\n");
              f();
              LOCK(mutex_fhdata,cg_thread_assert_not_locked(mutex_fhdata));
              e();
            }
          }/* !WITH_ASSERT_LOCK */
#undef p
#undef f
#undef e
#undef G
          return true;
        }/* if (f==...)*/
      }/* foreach_root*/
      return true;
    }/*if(op)*/
  }
  char *posHours=strstr(path,MAGIC_SFX_SET_ATIME);
  if (posHours){
    const int len=(int)(posHours-path);
    posHours+=sizeof(MAGIC_SFX_SET_ATIME)-1;
    char path2[MAX_PATHLEN];
    strncpy(path2,path,len)[len]=0;
    bool found;FIND_REALPATH(path2);
    log_debug_now("%s found=%d ",path2,found);
    if (found){
      cg_file_set_atime(RP(),&zpath->stat_rp,3600L*atoi(posHours));
      return true;
    }
  }/*posHours*/
  return false;
}/*trigger_files()*/
static int read_special_file(const int i, char *buf, const size_t size, const off_t offset){
  const char *content;
  LOCK(mutex_special_file,
       struct textbuffer b={0};special_file_content(&b,i);
       const int l=textbuffer_length(&b);
       const int n=MIN_int(size,l-(int)offset);
       textbuffer_copy_to(&b,offset,offset+n,buf));
  //       if (n>0) memcpy(buf,content+offset,n));
  return n<0?EOF:n;
}
static void make_info(){
  cg_thread_assert_locked(mutex_special_file);
  int l;
  while((l=print_all_info())>=_info_capacity){
    CG_REALLOC(char *,_info,_info_capacity=_info_capacity*2+0x100000+17);
  }
  _info_l=l;
}
// https://www.vbforums.com/showthread.php?557980-RESOLVED-2008-Force-a-refresh-of-all-Win-Explorer-windows-showing-a-particular-folder
// https://ccms-ucsd.github.io/GNPSDocumentation/fileconversion/
