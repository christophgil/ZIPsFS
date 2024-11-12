/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////

#define AUTOGEN_CMD_MAX 100
#define N_PATTERNS 9
#define _CA_FLAG_FSIZE_NOT_REMEMBER (1<<1) /* If _stdout is set to STDOUT_TO_MALLOC or STDOUT_TO_MMAP, no File is generated. The file size is normally saved in a hashmap and reused. */
#define _CA_FLAG_FSIZE_MULTIPLE (1<<2)
enum _autogen_capture_output{STDOUT_DROP,STDOUT_TO_OUTFILE,STDOUT_TO_MALLOC,STDOUT_TO_MMAP,STDOUT_MERGE_TO_STDERR};
struct _autogen_config{
  _AUTOGEN_CONFIG_INTERNAL_FIELDS;
  enum _autogen_capture_output out;
  double filesize;
  int concurrent_computations; /* If unset (recommended) or 1, it will be computed in a locked code block such that only one computation is performed at a time */
  bool no_redirect; /* Do not redirect the stdout / stderr. Was required for one Windows executable under wine. The process freezes when stdout ist redirected. */
  int flags; /* Bitmask of bits  named  _CA_FLAG_.. */
  char *patterns[N_PATTERNS]; /* Filter files with literal strings (No regex!). The patterns[0], [1], [2], ... are AND-ed and the colon ":" separated within patterns[i] are OR-ed */
  char *ends; /* Any ending must match */
  char *ends_ic; /* Same, ignore case */
  const char *ext; /* Extension of generated file */
  char *info; /* Will be printed into the logs */
  char **env; /* Environment. See execle(). Requires trailing NULL */
  char *cmd[]; /* Command line parameters. Requires trailing NULL */
};


/* -- Some test cases for our specific raw file projects. Testing the patterns filter --- */
#define C(s)  .ends=".raw",.patterns={"_30-0033_:_30-0037_",NULL},.filesize=s
struct _autogen_config
_test_malloc_fail= {C(  99),.ext=".test4.txt",.out=STDOUT_TO_OUTFILE,.cmd={"ls","-l","not exist",NULL},},
  _test_cmd_notexist={C(  99),.ext=".test5.txt",.out=STDOUT_TO_MALLOC,.cmd={"not exist",NULL},},  /* Yields  Operation not permitted */
  _test_textbuf=     {C(  99),.ext=".test6.txt",.cmd={NULL}};
#undef C


/* -- Configurations for mass spectrometry rawfiles --- */
struct _autogen_config
_wiff_strings={.ends=".wiff",.filesize=99999,.ext=".strings",.cmd={"bash","-c","tr -d '\\0' <"PLACEHOLDER_INFILE"|strings|grep '\\w\\w\\w\\w' # "PLACEHOLDER_TMP_OUTFILE" "PLACEHOLDER_TMP_OUTFILE,NULL}},
#define DOCKER_MSCONVERT_CMD "docker","run","-v",PLACEHOLDER_INFILE_PARENT":/data","-v",PLACEHOLDER_OUTFILE_PARENT":/dst","-it","--rm","chambm/pwiz-skyline-i-agree-to-the-vendor-licenses","wine","msconvert",PLACEHOLDER_INFILE_NAME,"--outdir","/dst", "--outfile",PLACEHOLDER_TMP_OUTFILE_NAME
#define _msconvert(extension,suffix,limit) {.info="Requires Docker",.ext=extension,.ends=suffix,.filesize=limit,.out=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT_CMD,NULL}}
  _msconvert_mzML=_msconvert(".mzML",".raw:.wiff",99999999999),
  _msconvert_mgf=_msconvert(".mgf",".raw:.wiff",99999999999),
  _wiff_scan={.ext=".scan",.ends=".wiff",.filesize=99999999999,.no_redirect=true,.cmd={"bash","rawfile_mk_wiff_scan.sh",PLACEHOLDER_INFILE,PLACEHOLDER_TMP_DIR,PLACEHOLDER_OUTFILE,NULL}};
//_msconvert_mzML={.info="Requires Docker",.ext=".mzML",.ends=".raw:.wiff",.filesize=99999999999,.out=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT,NULL}},
// _msconvert_mgf= {.info="Requires Docker",.ext=".mgf", .ends=".raw:.wiff",.filesize=99999999999,.out=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT,"--mgf",NULL}},
//  _wiff_scan={.ext=".scan",.ends=".wiff",.filesize=99999999999,.out=STDOUT_MERGE_TO_STDERR,.cmd={PLACEHOLDER_EXTERNAL_QUEUE,PLACEHOLDER_INFILE,PLACEHOLDER_OUTFILE,NULL}},


/* -- Configurations examples with Imagemagick image file conversion --- */

#define COMMON(op,out) .info="Requires Imagemagick",.flags=_CA_FLAG_FSIZE_MULTIPLE,.filesize=1,.cmd={"convert",PLACEHOLDER_INFILE,"-scale",op,out,NULL}
#define I(x) _##x##25={.ext=".scale25%."#x,.ends_ic="."#x, COMMON("%25","-"),.out=STDOUT_TO_MMAP }
struct _autogen_config I(jpeg), I(jpg), I(png), I(gif);
#undef I
#define I(x) _##x##50={.ext=".scale50%."#x,.ends_ic="."#x,COMMON("%50",PLACEHOLDER_TMP_OUTFILE) }
                                              struct _autogen_config I(jpeg),I(jpg),I(png),I(gif);
#undef I
#undef COMMON


                                                                                          /////////////////////////////////////////////
                                                                                          /// The list of all active configurations ///
                                                                                          /////////////////////////////////////////////

                                                                                          static struct _autogen_config **config_autogen_rules(){
                                                                                            static struct _autogen_config* aa[]={
                                                                                              &_test_cmd_notexist,
                                                                                              &_test_textbuf,
                                                                                              &_wiff_strings,
                                                                                              &_wiff_scan,
                                                                                              &_msconvert_mzML,
                                                                                              &_msconvert_mgf,
                                                                                              &_jpeg50,
                                                                                              &_jpg50,
                                                                                              &_png50,
                                                                                              &_gif50,
                                                                                              &_jpeg25,
                                                                                              &_jpg25,
                                                                                              &_png25,
                                                                                              &_gif25,
                                                                                              NULL};
                                                                                            return aa;
                                                                                          }
// see  FOREACH_AUTOGEN(i,s)


/// The tiny placeholder .wiff.scan files are replaced by larger valid files during conversion. ///
/// This function prevents that
static bool config_autogen_file_is_invalid(const char *path,const int path_l, struct stat *st, const char *rootpath){
  if (!st) return false;
  if (ENDSWITH(path,path_l,".wiff.scan") && st->st_size==44) return true; /*The tiny wiff.scan files are placeholders*/
  return  false;
}
////////////////////////////////////////////////////////////////////////
/// Before a file is generated, its future size can only be guessed. ///
/// The guessed size must not be smaller than the real size.         ///
////////////////////////////////////////////////////////////////////////
static long config_autogen_size_of_not_existing_file(const char *vp,const int vp_l, bool *cache_size){
  struct _autogen_config  *s=aimpl_struct_for_genfile(vp,vp_l);
  //log_entered_function("%s %p",vp,s);
  if (!s) return -1;
  *cache_size=!(s->flags&_CA_FLAG_FSIZE_NOT_REMEMBER);
  if (s->flags&_CA_FLAG_FSIZE_MULTIPLE){
    struct path_stat path_stat[AUTOGEN_MAX_DEPENDENCIES];
    long sum=0;
    RLOOP(i,autogen_find_sourcefiles(path_stat,vp,vp_l)) sum+=path_stat[i].stat.st_size;
    return sum*s->filesize;
  }
  return s->filesize;
}
static bool config_autogen_run(struct textbuffer *buf[], struct _autogen_config *s,  const char *outfile,const char *tmpoutfile,const char *logfile){ /*NOT_TO_HEADER*/
  if (s==&_test_textbuf){
    textbuffer_add_segment((buf[0]=textbuffer_new(MALLOC_autogen_textbuffer)),strdup("How is it going?"),sizeof("How is it going?"));
    return true;
  }
  return false;
}
