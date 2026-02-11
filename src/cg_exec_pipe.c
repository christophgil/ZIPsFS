#ifndef _cg_exec_pipe_dot_c
#define _cg_exec_pipe_dot_c
#include <zlib.h>
const char *_CMD_ZLIB[]={NULL,NULL};
#ifndef DEBUG_THROTTLE_DOWNLOAD
#define DEBUG_THROTTLE_DOWNLOAD 0
#endif

static long cg_read_gzip(const int in, const int out, void (*progress)(void*), void *progress_para){
  if (DEBUG_THROTTLE_DOWNLOAD) log_entered_function("DEBUG_THROTTLE_DOWNLOAD active");
  errno=0;
  gzFile gz=gzdopen(in,"rb");
  if (!gz){
    fprintf(stderr, "gzdopen failed\n");
    close(in);
    return -1;
  }
  char buf[1024*1024];
  long count=0;
  int n;
  for (int i=0;(n=gzread(gz,buf,sizeof(buf)-1))>0; i++){
    if (progress) progress(progress_para);
    cg_fd_write(out,buf,n);
    count+=n;
    //log_debug_now("read n:%d total:%ld",n,count);
    if (DEBUG_THROTTLE_DOWNLOAD){fputc('l',stderr); usleep(1000*1000);}
  }
  gzclose(gz); /* Do not close close(in) !!! */
  return n<0?-1:count;
}





#define E(msg) perror(__FILE__":"STRINGIZE(__LINE__)" "msg);
static bool cg_exec_pipe(const char  *cmd1[], const char  *env[], const char *cmd2[], const int fdout,  void (*progress)(void*), void *progress_para){
  //log_entered_function("cg_exec_pipe %s   %s",cmd1?*cmd1:NULL, cmd2?*cmd2:NULL);
  int pipefd[2];
  if (pipe(pipefd)==-1){ E("pipe");return false;}
  const int pid=fork();
  if (pid<0){ E("fork()"); return false;}
  if (!pid){
    close(pipefd[1]);
    int ev=0;
    if (cmd2==_CMD_ZLIB){
      const long nread=cg_read_gzip(pipefd[0],fdout,progress,progress_para);
      ev=nread>0?0:-nread;
    }else{
      ev=cg_exec(cmd2,env,pipefd[0],fdout,0);
    }
    exit(ev);
  }
  close(pipefd[0]);
  const bool ok=cg_fork_exec(cmd1,env,0,pipefd[1],0);
  close(pipefd[1]);/* Reader will see EOF */
  int wstatus=0;
  waitpid(pid,&wstatus,0);
  const int e=WIFEXITED(wstatus)?WEXITSTATUS(wstatus):0;
  if (e){ fprintf(stderr,__FILE__":"STRINGIZE(__LINE__)" cg_exec_pipe()  exited, status=%d  %s\n",e, cg_error_symbol(e)); perror(""); return false;}
  return ok;
}
#undef E








#endif // _cg_exec_pipe_dot_c

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#include "cg_download_file.c"

int main(int argc, char *argv[]){

  if (0) {
    const char *cmd1[]={"ls","-l",NULL};
    const char *cmd2[]={"grep","-F",".c",NULL};
    cg_exec_pipe(cmd1,NULL,cmd2,0,NULL,NULL);
    exit(0);
  }





  if (1){
    const char *url="ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.gz";
    const char *outfile="/home/cgille/tmp/test_out";
    bool ok=cg_download_url2(COMPRESSION_GZ, url, outfile, NULL,NULL);
    log_exited_function("%s  ok: %s",outfile,success_or_fail(ok));
    exit(0);
  }



  if (1){
    // curl  -o  -  ftp://ftp.uniprot.org/pub/databases/uniprot/LICENSE
    const char *cmd1[]={"curl","-o","-","ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.gz",NULL};
    const char *cmd2[]={"gzip","-dc",NULL};
      const char *outfile="/home/cgille/tmp/test_out";
      const int fdout=open(outfile,O_WRONLY|O_CREAT|O_TRUNC,0644);
      if (fdout==-1) { log_errno("open(%s)",outfile); return false;}
      bool ok=cg_exec_pipe(cmd1,NULL,_CMD_ZLIB,fdout,NULL,NULL);
      log_exited_function("%s  ok: %s",outfile,success_or_fail(ok));
      exit(0);
  }


  if (1){
    // curl  -o  -  ftp://ftp.uniprot.org/pub/databases/uniprot/LICENSE
    const char *cmd1[]={"curl","-o","-","ftp://ftp.uniprot.org/pub/databases/uniprot/LICENSE",NULL};
    const char *cmd2[]={"tr","a","A",NULL};
    cg_exec_pipe(cmd1,NULL,cmd2,0,NULL,NULL);
    exit(0);
  }
  return 0;
}

#endif //__INCLUDE_LEVEL__
