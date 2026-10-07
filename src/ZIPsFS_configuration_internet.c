////////////////////////////////////
/// cccccCOMPILE_MAIN=ZIPsFS          ///
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




static void _remove_redundant_pathcomponent(char *hardlink){
  bool modified=false;
  RLOOP(repeat,5){
    const char *filename=strrchr(hardlink,','); /* Slash has been converted to comma */
    if (!filename) break;
    filename++;
    for(char *b=hardlink; b && b<filename-1; b=strchr(b+1,',')){
      if (*b==',') b++;
      char *e=strchr(b+1,',');
      *e=0;
      const bool found=strstr(filename,b);
      *e=',';
      if (found){
        cg_strncpy0(b,e+1,PATH_MAX);
        modified=true;
      }
    }
    if (!modified) break;
  }
}

/****************************************************************************************************************/
/* When downloading a file into /zipsfs/n from the internet,                                                    */
/* a hardlink may be created containing with a version in the name.                                             */
/* If the function returns  true, the file will be set read-only                                                */
/****************************************************************************************************************/
static bool config_internet_hardlink_filename(char *lnk, char *additional_link, const int max_l, const char *url,const time_t header_time){
  if (url){
    char *d=lnk;
    *d=0;
    const int url_l=strlen(url);
    const char *url_e=url+url_l;
    char txt[999];*txt=0;
    const bool isFasta=url_l>10 && (!strcmp(".fasta",url_e-6) || !strcmp(".fasta.gz",url_e-9));
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
        while(d<lnk+INTERNET_VERSION_MAX && *s && !isspace(*s)) *d++=*s++;
      }
    }
    if (d==lnk){
      time_t t=header_time;
      d+=strftime(d,33,"%Y%m%d",gmtime(&t));
    }
    *d++='_';
    const char *s=shorter?shorter:url;
    if (d+strlen(s)>=lnk+max_l-1){
      warning(WARN_NET,url,"Exceeding max path-length for hard link.");
      *lnk=0;
    }else{
      for(;*s;s++,d++) *d=*s=='/'||*s==':'?',':*s;
      *d=0;
    }
    _remove_redundant_pathcomponent(lnk);
  }
  cg_copy_path(additional_link,"~/MS/Keep");
  if (url) strcat(strcat(additional_link,"/"),lnk);
  //log_exited_function("lnk: '%s'  additional_link=%s\n",lnk,additional_link);
  return false; /* true for read-only */
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
  char hardlink[PATH_MAX], additional_link[PATH_MAX];
  char *uu[]={
    "ftp://ftp.expasy.org/databases/uniprot/current_release/knowledgebase/reference_proteomes/Viruses/UP001733680/UP001733680_2006137.fasta",
    "https://ftp.uniprot.org/pub/databases/uniprot/current_release/"A,
    "ftp://ftp.expasy.org/databases/uniprot/CURRENT_RELEASE/"A,
    "ftp://ftp.expasy.org/databases/uniprot/current_release/"A,
    "ftp://ftp.expasy.org/databases/uniprot/current_release/"A".gz",
    "ftp://ftp.uniprot.org/pub/databases/uniprot/current_release/"A,
    "ftp://ftp.ebi.ac.uk/pub/databases/uniprot/current_release/"A,
    "ftp://localhost/hello.txt",
    NULL};
#undef A

  config_internet_hardlink_filename(hardlink,additional_link,PATH_MAX,NULL,0);
  fprintf(stderr,"additional_link=%s",additional_link); //exit(99);

  for(char **u=(char**)uu; u&&*u; u++){
    config_internet_hardlink_filename(hardlink,additional_link,PATH_MAX,*u,time(NULL));
    printf("\n"ANSI_INVERSE"%s"ANSI_RESET"  ->\n  %s and %s    %s\n\n",*u,hardlink,additional_link,*hardlink?GREEN_SUCCESS:RED_FAIL);
  }
  return 0;
}
#endif
// ftp,,,ftp.expasy.org,databases,uniprot,current_release,knowledgebase,reference_proteomes,Viruses,UP001733680,UP001733680_2006137.fasta
