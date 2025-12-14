#!/usr/bin/env bash
#################################################################
### COMPILE_MAIN=ZIPsFS                                       ###
#################################################################


set -u
########################
### Please customize ###
########################
LL="-lpthread -lm -lzip -lz "
SEARCH_SO_FILES='/usr/local/lib/x86_64-linux-gnu /usr/pkg/lib /usr/local/lib /opt/local/lib /usr/lib /opt/ooce/lib/amd64 /lib'
SEARCH_INCLUDE_FILES='/usr/include/fuse3 /usr/local/include/fuse3  /usr/local/include /opt/local/include /usr/pkg/include /local/libfuse-master/include /opt/ooce/include /opt/ooce/libzip/include /usr/include/fuse/'
#############
### Help  ###
#############
readonly ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m'  ANSI_RESET=$'\e[0m' ANSI_UNDERLINE=$'\e[4m'
RUN_MAIN=1
print_help(){
    sed 's|^\.$||1'<<EOF
.
${ANSI_INVERSE}ZIPsFS installer$ANSI_RESET
.
  This script compiles ZIPsFS in an OS independent way. It has been tested on
    - 64bit Linux
    - 64bit FreeBSD
    - 64bit NetBSD,
    - 64bit MacOSX
    - 64bit Omnios. It is an Illumos distribution and a decendent of Solaris.
.
.
  There are slight differences between different UNIX operation systems.
  These are probed and respective macro definition like -DHAS_ADDR2LINE=1
  are passed to the compiler for conditional compilation.
.
  Also the FUSE version is detected.  Different versions have different  numbers of parameters.
.
  Why a Bash script and not autoconf?
    - Autoconf is very complex
    - Bash is widely known and this script can  easily adapted  by users.
    - For the simple case (fuse3, Linux), the typical sequence will work. See the github page:
.
        ./configure && make
.
${ANSI_INVERSE}Options:$ANSI_RESET
.
  -h  Print this text
.
  -g  Take ${ANSI_UNDERLINE}gcc$ANSI_RESET instead of ${ANSI_UNDERLINE}clang$ANSI_RESET
.
  -s  Compilation without sanitizer.
.
      The sanitizer  is normally  activated unless probing reveals that it is not working.
.
      ${ANSI_UNDERLINE}Advantage:$ANSI_RESET
        * The sanitizer identifies memory violations and other bugs in ZIPsFS.
.
      ${ANSI_UNDERLINE}Disadvantages:$ANSI_RESET
         *  ZIPsFS is slower and uses up more CPU cycles.
         *  With glibc < 2.38  the sanitizer caused a memory leak when opening directories (opendir) for reading.
            Check your version with ldd --version.
.
      ${ANSI_UNDERLINE}Reports a bug:$ANSI_RESET
          Please send the stack output and include the source code.
          The line numbers in the error output refer to your version of ZIPsFS.
.
      ${ANSI_UNDERLINE}Line numbers in stack traces:$ANSI_RESET
          Whether stack traces have line numbers can be tested with the option -T:
.
                ZIPsFS -T 1
.
          If addr2line fails to report line numbers,  a lower compiler version can be tried.
          For me installing clang-13 and modifying the executable search path solved the problem:
.
               apt-get install install clang-13
               export PATH=/usr/lib/llvm-13/bin:$PATH
               $0
.
  -F  Absolute path to libfuse.so file for the case that the compiler does not find it. Example:
.
          -F /local/illumos-sshfs-master/libfuse/proto/usr/lib/amd64/libfuse.so.2.7.1
.
  -Z  Absolute path to libzip.so file.
.
  -R  Absolute paths for dynamic linking.
      Activate this option if libraries are not found at runtime.
      It uses the option ${ANSI_UNDERLINE}-rpath$ANSI_RESET of /usr/bin/ld.
.
EOF
}

####################
### Directories  ###
####################
SRC="${1:-}"
[[ -z $SRC ]] && SRC=${BASH_SOURCE[0]}
DIR=${SRC%/*}
DIR=${DIR%%/}
[[ $DIR != /* ]] && DIR=$PWD/$DIR
TEMP=~/tmp/ZIPsFS/compilation
! mkdir -p $TEMP && press_enter
for f in $TEMP/*; do
    [[ -e $f && $SRC -nt $f ]] && rm "$f"
done

#############################
### Command line options  ###
#############################
LIBFUSE=''
LIBZIP=''
WITH_SANITIZER=1
WITH_PROFILER=0
WITH_RPATH=0
CCOMPILER=${CC:-gcc}
OPT_PROFILER=''

CC_OPTS='-D_FILE_OFFSET_BITS=64'


clang --version >/dev/null &&  CCOMPILER=clang


press_enter(){
    local what=${1:-continue}
    echo >&2
    read -r -p  "Press Enter to $what"
}
while getopts 'RhsgF:Z:' o; do
    case $o in
        R) WITH_RPATH=1;;
        s) WITH_SANITIZER=0;;
        g) CCOMPILER=gcc;;
        F) LIBFUSE=$(realpat2h $OPTARG);;
        Z) LIBZIP=$(realpath $OPTARG);;
        h) print_help;RUN_MAIN=0;;
        *) echo "wrong_option $o"; press_enter;;
    esac
done
shift $((OPTIND-1))
IS_CG=0; [[ $(hostname). == s-mcpb-ms0* && $USER == cgille ]] && IS_CG=1 && echo 'Is PC of developer'
if ((IS_CG)); then
    export PATH=/usr/lib/llvm-13/bin:/usr/lib/llvm-14/bin/:$PATH
    WITH_SANITIZER=0
    # WITH_PROFILER=1
fi



$CCOMPILER --version |head -n 1
sanitize=''


if  ((WITH_SANITIZER)); then
    sanitize='-rdynamic -fsanitize=address '
    if [[ $CCOMPILER == clang ]]; then
        sanitize+='-fno-omit-frame-pointer '
    else
        sanitize+='-static-libasan '
    fi
    ASAN_OPTIONS='alloc_dealloc_mismatch=0:detect_leaks=0:detect_odr_violation=0'
    ASAN_OPTIONS='detect_leaks=0'
fi


ostype=$(echo $OSTYPE |tr '[:upper:]' '[:lower:]') # ${OSTYPE,,} not portable
if [[ $WITH_RPATH == 0 && ( $ostype == netbsd || $ostype == solaris*) ]]; then
    WITH_RPATH=1
    echo -e  "\nNote: The command line option -R is automatically activated for $OSTYPE. Run $0 -h for help.\n"
    press_enter
fi

########################################################################
### Try compilation tiny code to probe the availability of features  ###
########################################################################
try_compile(){
    local success=1 name=$1  includes="$2"  main="$3"  cc_opts="${4:-}" ld_opts="${5:-}"
    local x=$TEMP/$name
    local c=$x.c
    [[ ! -s $c ]] && echo -e "$includes\nint main(int argc,char *argv[]){\n $main; }\n" >$c.$$.tmp && mv $c.$$.tmp $c
    local cmd="$CCOMPILER $CC_OPTS $IPATHS  $cc_opts  $c $ld_opts $LIBFUSE $LIBZIP $RPATHS $LPATHS $LL   -o $x"
    ! { echo $cmd; echo; $cmd  2>&1; } >$x.log  && success=0 && echo "Probing failed$ANSI_FG_RED:$ANSI_RESET See   '$x.log'" >&2
    ((success)) &&  echo "${ANSI_FG_GREEN}Probing succeeded$ANSI_RESET '$x.log'" >&2
    echo $success # Used for HAS_XXX=1 or 0

    return $((success==0))
}
####################
### Functions    ###
####################
dirpaths_with_pfx(){
    local pfx=$1 d
    shift
    for d in "$@"; do [[ -d $d ]] && echo -n "$pfx$d"; done
}
detect_fuse_version(){
    local p
    for p in -lfuse3 -lfuse2 -lfuse1 -lfuse ''; do
        [[ -n $LIBFUSE && -n $p ]] && continue
        local x=$TEMP/fuse$p
        if try_compile ${x##*/} $'#define FUSE_USE_VERSION 33\n#include <fuse.h>' 'if (!argc) fuse_main(0,NULL,NULL,NULL);'  "$IPATHS" "$p $LPATHS" >/dev/null; then
            $x>&2  && echo " $p " && return 0
            echo "${ANSI_FG_RED}Problem$ANSI_RESET running $x" >&2
            press_enter
        fi
    done
    echo "${ANSI_FG_RED}Problem $ANSI_RESET compiling with libfuse. Is it install? Possiple package names 'libfuse3-dev', 'libfuse3', 'libfuse-dev' or  'libfuse'" >&2
    press_enter
    return 1
}
print_linker_option_execinfo(){
    local p
    for p in '' -lexecinfo; do
        try_compile "backtrace$p" '#include <execinfo.h>' 'backtrace_symbols(0,0);' $p >/dev/null && echo $p && return 0
    done
    return 1
}
find_bugs(){
    local cc=$(find $DIR -name '*.c')

    grep -n  '^ *[a-z].*) *LOCK_N(' $cc && echo 'Error: LOCK_N(...) requires curly braces' && exit 1
    local hh=$(find $DIR -name '*.h')
    grep -F -w -e 'strchrnul' -e 'group_member' -e 'strcasestr' -e 'memmem'  $cc $hh && echo 'Not supported on all platforms' && exit 1

}
######################
### main function  ###
######################
main(){
    readonly D=' -D'
    IPATHS=$(dirpaths_with_pfx ' -I' $SEARCH_INCLUDE_FILES)
    LPATHS=$(dirpaths_with_pfx ' -L' $SEARCH_SO_FILES)
    RPATHS=''
    ((WITH_RPATH)) && RPATHS=$(dirpaths_with_pfx : $SEARCH_SO_FILES)
    [[ -n $RPATHS ]] && RPATHS="-Wl,-rpath=${RPATHS#:}"
    if ((IS_CG)); then
        cd || return 1 # Otherwise the logs contain relative paths
        find_bugs

    fi
    ! try_compile 'empty' '' '' >/dev/null && echo "Cannot compile simple file $TEMP/empty.c" && return 1
    ((WITH_SANITIZER)) && ! try_compile 'sanitize' '' '' "$sanitize" >/dev/null && sanitize='' && echo "${ANSI_FG_RED}Note: Sanitizer not working$ANSI_RESET"
    local lib
    for lib in $LIBZIP $LIBFUSE; do
        [[ -n $lib && $lib != -*  ]] && ! ls -l -- $lib  && return 1
    done
    local opts="$OPT_PROFILER"   NOWARN="-Werror-implicit-function-declaration -Wno-string-compare " fuse_version
    #    -Wno-format-overflow  -Wno-format-truncation   -Wno-format-zero-length

    ok=0; addr2line -H >/dev/null 2>/dev/null     && ok=1; opts+="${D}HAS_ADDR2LINE=$ok";
    ok=0; atos -h 2>/dev/null                     && ok=1; opts+="${D}HAS_ATOS=$ok"
    ok=0; LL+=" $(print_linker_option_execinfo) " && ok=1; opts+="${D}HAS_BACKTRACE=$ok"
    ! fuse_version=$(detect_fuse_version) && echo "$ANSI_FG_RED Failed to compile tiny test program with libfuse $ANSI_RESET"  && return 1
    LL+=" ${fuse_version} "
    opts+="${D}HAS_EXECVPE=$(             try_compile execvpe                 '#include <unistd.h>'       'execvpe("",NULL,NULL);'              -Werror)"
    opts+="${D}HAS_UNDERSCORE_ENVIRON=$(  try_compile us_environ              '#include <unistd.h>'       'extern char **_environ; char **ee=_environ;')"
    opts+="${D}HAS_ST_MTIM=$(             try_compile st_mtim                 '#include <sys/stat.h>'     'struct stat st;  st.st_mtim=st.st_mtim    ;')"
    opts+="${D}HAS_POSIX_FADVISE=$(       try_compile posix_fadvise           '#include <fcntl.h>'        'posix_fadvise(0,0,0,POSIX_FADV_DONTNEED)  ;')"
    opts+="${D}HAS_DIRENT_D_TYPE=$(       try_compile dirent_d_type           '#include <dirent.h>'       'struct dirent *e; e->d_type=e->d_type     ;')"
    opts+="${D}HAS_RLIMIT=$(              try_compile rlimit_3                '#include <sys/resource.h>' 'struct rlimit l; getrlimit(RLIMIT_AS,&l)     ;')"
    opts+="${D}HAS_NO_ATIME=$(            try_compile no_atime                $'#define _GNU_SOURCE\n#include <sys/statvfs.h>' 'struct statvfs s; int i=s.f_flag&ST_NOATIME;')"
    local x=${DIR%/*}/ZIPsFS
    rm  "$x" 2>/dev/null
    [[ -s $x ]] && echo "${ANSI_RED}Warning, $x exists $ANSI_RESET"
    {
        ! pushd $DIR >/dev/null && return
        local cc="$(sed -n 's|^#include "\(.*\.c\)"$|\1|p' <ZIPsFS.c) $DIR/ZIPsFS.c"

        local g=$DIR/generated_ZIPsFS.inc
        cat $cc | sed 's| *// cppcheck-suppress.*||1' | sed -n 's|^\(static .*)\) *{$|\1;|p' |grep -v -e NOT_TO_HEADER -e '\[[A-Z]' -e '\<IF1('  >$g
        if  ((WITH_PROFILER)); then
            opts+=" ${D}WITH_PROFILER=1 "
            {
                grep 'PROFILED(' $g
                cat <<EOF
int stat(const char *path,struct stat *statbuf);
int lstat(const char *path,struct stat *statbuf);
DIR *opendir(const char *name);
zip_int64_t zip_get_num_entries(zip_t *archive, zip_flags_t flags);
EOF
            } | ./cg_profiler_make_files.sh
        fi
        popd >/dev/null
    }



    local c=${x}_compilation.sh
    {
        echo export ASAN_OPTIONS="'${ASAN_OPTIONS:-}'"
        echo "# This file has been created with $SRC"$'\n\n'$CCOMPILER $CC_OPTS  $NOWARN "$opts" $IPATHS -O0 -g  $sanitize  $DIR/ZIPsFS.c  $LIBFUSE $LIBZIP $RPATHS   $LPATHS $LL  -o $x
    }|tee $c
    chmod +x $c
    ls -l $c

    press_enter "proceed compilation"
    . $c
    more $c |grep -v '^#'
    if ls -l -h $x; then
        echo -e "$ANSI_FG_GREEN Success $ANSI_RESET" $'\n\nSuggest testing:\n'$DIR/ZIPsFS_testing.sh $x $'\n'
        [[ $OSTYPE == *freebsd* ]] && echo 'If ZIPsFS does not work, try as root'

        local pfx=''
        if ((WITH_SANITIZER))  && setarch -h >/dev/null 2>/dev/null && pfx=$(uname -m); then
            pfx="setarch $pfx -R"   # This prevents segmentation fault with sanitizer
            ! $pfx date && pfx=0
        fi
        set -x
        $pfx $x --version
        set +x
    else
        echo "$ANSI_FG_RED Failed $ANSI_RESET"
    fi
}
((RUN_MAIN)) && main "$@"
