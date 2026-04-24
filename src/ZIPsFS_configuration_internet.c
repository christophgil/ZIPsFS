////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS          ///
/// Dynamically downloaded files ///
////////////////////////////////////

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#include "cg_stacktrace.c"
#include <zlib.h>
#include "cg_utils.c"
#include "cg_exec_pipe.c"
#include "cg_download_file.c"
#define warning(...)
#endif
enum { INTERNET_VERSION_MAX=33};


/****************************************************************************************************************/
/* When downloading a file into /zipsfs/n from the internet,                                                    */
/* a hardlink may be created containing with a version in the name.                                             */
/****************************************************************************************************************/
static bool config_internet_hardlink_filename(char *lnk_name, char *additional_link_dir, const int max_l, const char *url,const time_t header_time){
  char *d=lnk_name;
  *d=0;
  const int url_l=strlen(url);
  const char *url_e=url+url_l;
  char txt[999];*txt=0;
  const bool isFasta=!strcmp(".fasta",url_e-6) || !strcmp(".fasta.gz",url_e-9);
#define R "/uniprot/current_release/"
  char *shorter=strstr(url,R);  if (shorter) shorter+=sizeof(R)-1;
#undef R
  if (shorter && isFasta){
    char relnotes[url_l+16];strcpy(relnotes,url); strcpy(relnotes+(shorter-url),"relnotes.txt");
    log_verbose("Going to open '%s' ...",relnotes);
    const int nread=cg_load_url(txt, sizeof(txt)-1,relnotes);
#define S "UniProt Release "
    const char *s=nread<=0?NULL:strstr(txt,S);
    if (s){
      s+=sizeof(S)-1;
#undef S
      while(*s && isspace(*s)) s++;
      while(d<lnk_name+INTERNET_VERSION_MAX && *s && !isspace(*s)) *d++=*s++;
    }
  }
  if (d==lnk_name){
    time_t t=header_time;
    d+=strftime(d,33,"%Y%m%d",gmtime(&t));
  }
  *d++='_';
  const char *s=shorter?shorter:url;
  if (d+strlen(s)>lnk_name+max_l){
    warning(WARN_NET,url,"Exceeding max path-length for hard link.");
    *lnk_name=0;
  }else{
    for(;*s;s++,d++) *d=*s=='/'||*s==':'?',':*s;
    *d=0;
  }
  strcpy(additional_link_dir,"/home/x/MS/Keep");
  log_exited_function("lnk_name: '%s'\n",lnk_name);
  return true;
}




/********************************************/
/* Protect certain files from being deleted */
/********************************************/
static bool config_internet_must_not_delete(const char *filename, const int filename_l){
  const char *e=filename+filename_l;
  const bool ret=
    isdigit(*filename) &&
    (!strcmp(e-6,".fasta") || !strcmp(e-9,".fasta.gz")) &&
    strstr(filename,",reference_proteomes,");
  log_exited_function("filename: '%s' %d  ",filename,ret);
  return ret;
}
/* ================================================================================ */
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
// wget -O - ftp://ftp.expasy.org/databases/uniprot/current_release/relnotes.txt
// ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/knowledgebase/reference_proteomes/Eukaryota/UP000005640/UP000005640_9606.fasta
// ftp://ftp.expasy.org/databases/uniprot/current_release/knowledgebase/reference_proteomes/Eukaryota/UP000005640/UP000005640_9606.fasta
// ftp://ftp.uniprot.org/pub/databases/uniprot/current_release/knowledgebase/reference_proteomes/Eukaryota/UP000005640/UP000005640_9606.fasta
//
// mnt/zipsfs/n/ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,reference_proteomes,Eukaryota,UP000005640,UP000005640_9606.fasta
// mnt/zipsfs/n/ftp,,,ftp.expasy.org,databases,uniprot,current_release,knowledgebase,reference_proteomes,Eukaryota,UP000005640,UP000005640_9606.fasta
// mnt/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,current_release,knowledgebase,reference_proteomes,Eukaryota,UP000005640,UP000005640_9606.fasta

int main(int argc,char *argv[]){
#define A "knowledgebase/reference_proteomes/Eukaryota/UP000005640/UP000005640_9606.fasta"
  char hardlink[PATH_MAX+1];
  char *uu[]={
      "https://ftp.uniprot.org/pub/databases/uniprot/current_release/"A,
      "ftp://ftp.expasy.org/databases/uniprot/CURRENT_RELEASE/"A,
      "ftp://ftp.expasy.org/databases/uniprot/current_release/"A,
      "ftp://ftp.expasy.org/databases/uniprot/current_release/"A".gz",
      "ftp://ftp.uniprot.org/pub/databases/uniprot/current_release/"A,
      "ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/"A,
      "ftp://localhost/hello.txt",
    NULL};
#undef A
  for(char **u=(char**)uu; u&&*u; u++){
    config_internet_hardlink_filename(hardlink,PATH_MAX,*u,time(NULL));
    printf("\n"ANSI_INVERSE"%s"ANSI_RESET"  ->\n   %s  %s\n\n",*u,hardlink,*hardlink?GREEN_SUCCESS:RED_FAIL);
  }
  return 0;
}
// ST_MTIMESPEC ST_MTIME
// #define ST_MTIME(st) strtok(ctime(&(st)->st_mtime),"\n")




#endif
