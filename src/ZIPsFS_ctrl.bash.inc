static const char *orig="1701163505\n\
#!/usr/bin/env bash\n\
readonly CLR_CACHE='Clear-cache' CANCEL='Cancel'\n\
INTERRUPT_THREAD=Interrupt-thread  BLOCK_THREAD=Block-thread KILL=Kill-ZIPsFS\n\
dir=${BASH_SOURCE[0]}\n\
dir=${dir%/*}\n\
yes_no(){\n\
read -p \"Are you sure to $1 [y/n] ?\" -n 1 -r\n\
[[ ${REPLY,,} == y ]] && return 0\n\
echo Canceled\n\
return 1\n\
}\n\
askWhichThread(){\n\
cat <<EOF\n\
S  PTHREAD_STATQUEUE\n\
M  PTHREAD_MEMCACHE\n\
D  PTHREAD_DIRCACHE\n\
EOF\n\
read -p 'What thread [S/M/D]?' -n 1 -r\n\
echo -n _$REPLY\n\
}\n\
my_stat(){\n\
local what=\"$1\"\n\
local f=\"$2\"\n\
if yes_no \"$what\"; then\n\
stat \"$f\" 2>/dev/null\n\
echo\n\
echo \"$what  done.\"\n\
fi\n\
}\n\
main(){\n\
local FOR_ALL=1\n\
((FOR_ALL)) && INTERRUPT_THREAD='' && BLOCK_THREAD='' && KILL=''\n\
select s in $CANCEL $CLR_CACHE $INTERRUPT_THREAD $BLOCK_THREAD $KILL;do\n\
case $s in\n\
$CANCEL) break;;\n\
$CLR_CACHE) my_stat 'clear the cache'  $dir/../FILE_CTRL_CLEARCACHE;break;;\n\
$INTERRUPT_THREAD) my_stat 'Interrupt a thread' $dir/../FILE_DEBUG_CANCEL$(askWhichThread); break;;\n\
$BLOCK_THREAD) my_stat 'Block a thread' stat $dir/../FILE_DEBUG_BLOCK$(askWhichThread); break;;\n\
$KILL) my_stat 'Kill ZIPsFS' $dir/../FILE_DEBUG_KILL;break;;\n\
esac\n\
done\n\
}\n\
\n\
main \"$@\"\n\
";
