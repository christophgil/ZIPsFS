/*
  Copyright (C) 2023
  christoph Gille
  This program can be distributed under the terms of the GNU GPLv3.
*/
#define ANSI_RED "\x1B[41m"
#define ANSI_MAGENTA "\x1B[45m"
#define ANSI_GREEN "\x1B[42m"
#define ANSI_BLUE "\x1B[44m"
#define ANSI_YELLOW "\x1B[43m"
#define ANSI_CYAN "\x1B[46m"
#define ANSI_WHITE "\x1B[47m"
#define ANSI_BLACK "\x1B[40m"
#define ANSI_FG_GREEN "\x1B[32m"
#define ANSI_FG_RED "\x1B[31m"
#define ANSI_FG_MAGENTA "\x1B[35m"
#define ANSI_FG_GRAY "\x1B[30;1m"
#define ANSI_FG_BLUE "\x1B[34;1m"
#define ANSI_FG_BLACK "\x1B[100;1m"
#define ANSI_FG_YELLOW "\x1B[33m"
#define ANSI_FG_WHITE "\x1B[37m"
#define ANSI_INVERSE "\x1B[7m"
#define ANSI_BOLD "\x1B[1m"
#define ANSI_UNDERLINE "\x1B[4m"
#define ANSI_RESET "\x1B[0m"
#define TERMINAL_CLR_LINE "\r\x1B[K"

#define log_struct(st,field,format)   printf("    " #field "=" #format "\n",st->field)
void prints(char *s){ if (s) fputs(s,stdout);}
#define log(...)  printf(__VA_ARGS__)
#define log_already_exists(...) (prints(" Already exists "ANSI_RESET),printf(__VA_ARGS__)
#define log_failed(...)  prints(ANSI_FG_RED" Failed "ANSI_RESET),printf(__VA_ARGS__)
#define log_warn(...)  prints(ANSI_FG_RED" Warn "ANSI_RESET),printf(__VA_ARGS__)
#define log_error(...)  prints(ANSI_FG_RED" Error "ANSI_RESET),printf(__VA_ARGS__)
#define log_succes(...)  prints(ANSI_FG_GREEN" Success "ANSI_RESET),printf(__VA_ARGS__)
#define log_debug_now(...)   prints(ANSI_FG_MAGENTA" Debug "ANSI_RESET),printf(__VA_ARGS__)
#define log_entered_function(...)   prints(ANSI_INVERSE"> > > >"ANSI_RESET),printf(__VA_ARGS__)
#define log_exited_function(...)   prints(ANSI_INVERSE"< < < <"ANSI_RESET),printf(__VA_ARGS__)
#define log_seek_ZIP(delta,...)   printf(ANSI_FG_RED""ANSI_YELLOW"SEEK ZIP FILE:"ANSI_RESET" %'16ld ",delta),printf(__VA_ARGS__)
#define log_seek(delta,...)  printf(ANSI_FG_RED""ANSI_YELLOW"SEEK REG FILE:"ANSI_RESET" %'16ld ",delta),printf(__VA_ARGS__)
#define log_cache(...)  prints(ANSI_RED"CACHE"ANSI_RESET" "),printf(__VA_ARGS__)
int isPowerOfTwo(unsigned int n){ return n && (n&(n-1))==0;}
void _log_mthd(char *s,int count){ if (isPowerOfTwo(count)) log(" %s=%d ",s,count);}
#define log_mthd_invoke(s) static int __count=0;_log_mthd(ANSI_FG_GRAY #s ANSI_RESET,++__count);
#define log_mthd_orig(s) static int __count_orig=0;_log_mthd(ANSI_FG_BLUE #s ANSI_RESET,++__count_orig);
