/*
  Copyright (C) 2023
  christoph Gille
  This program can be distributed under the terms of the GNU GPLv3.
*/
#define ANSI_RED "\x1B[41m"
#define ANSI_MAGENTA "\x1B[45m"
#define ANSI_GREEN "\x1B""[42m"
#define ANSI_BLUE "\x1B""[44m"
#define ANSI_YELLOW "\x1B""[43m"
#define ANSI_WHITE "\x1B""[47m"
#define ANSI_BLACK "\x1B""[40m"
#define ANSI_FG_GREEN "\x1B""[32m"
#define ANSI_FG_RED "\x1B""[31m"
#define ANSI_FG_MAGENTA "\x1B""[35m"
#define ANSI_FG_GRAY "\x1B""[30;1m"
#define ANSI_FG_BLUE "\x1B""[34;1m"
#define ANSI_FG_BLACK "\x1B""[100;1m"
#define ANSI_FG_YELLOW "\x1B""[33m"
#define ANSI_FG_WHITE "\x1B""[37m"
#define ANSI_INVERSE "\x1B""[7m"
#define ANSI_BOLD "\x1B""[1m"
#define ANSI_UNDERLINE "\x1B""[4m"
#define ANSI_RESET "\x1B""[0m"
#define TERMINAL_CLR_LINE "\r\x1B""[K"

#define log_struct(st,field, format)   printf("    " #field "=" #format "\n",st->field)

void log_reset(){ prints(ANSI_RESET);}
#define log_msg(...) printf(__VA_ARGS__)
#define log_already_exists(...) (prints(" Already exists "ANSI_RESET),printf(__VA_ARGS__)
#define log_failed(...)  prints(ANSI_FG_RED" Failed "ANSI_RESET),printf(__VA_ARGS__)
#define log_warn(...)  prints(ANSI_FG_RED" Warn "ANSI_RESET),printf(__VA_ARGS__)
#define log_error(...)  prints(ANSI_FG_RED" Error "ANSI_RESET),printf(__VA_ARGS__)
#define log_succes(...)  prints(ANSI_FG_GREEN" Success "ANSI_RESET),printf(__VA_ARGS__)
#define log_debug_now(...)   prints(ANSI_FG_MAGENTA" Debug "ANSI_RESET),printf(__VA_ARGS__)
#define log_entered_function(...)   prints(ANSI_INVERSE">>>> "ANSI_RESET),printf(__VA_ARGS__),putchar('\n');
#define log_exited_function(...)   prints(ANSI_INVERSE"<<<< "ANSI_RESET),printf(__VA_ARGS__),putchar('\n');
void log_path(const char *f,const char *path){
  printf("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,my_strlen(path));
}

#define log_abort(...)  { log_error(__VA_ARGS__),puts("Going to exit ..."),exit(-i); }

int log_func_error(char *func){
  int ret=-errno;
  log_error(" %s: %s\n",func, strerror(errno));
  return ret;
}


void log_strings(const char *pfx, char *ss[],int n,char *ss2[]){
  int count=0,i;
  printf(ANSI_UNDERLINE"%s"ANSI_RESET" %p\n",pfx,ss);
  if (ss){
    for(i=0;i<n;i++){
      printf("   %s %3d./.%d "ANSI_FG_BLUE"'%s'",pfx,i,n,ss[i]);
      if (ss2) prints(ss2[i]);
      puts(ANSI_RESET);
    }
  }
}


void log_zip_path(char *msg, struct zip_path zp){
  prints(ANSI_UNDERLINE);
  prints(msg);
  puts(ANSI_RESET);
  printf("      path="ANSI_FG_BLUE"='%s   %u byte\n",zp.path,my_strlen(zp.path));
  char *n=(char*)zp.zstat.name;

  if (n) printf("    zentry="ANSI_FG_BLUE"='%s   %u byte\n",n,my_strlen(n));
}
