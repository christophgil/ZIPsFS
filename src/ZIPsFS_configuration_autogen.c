/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////

#define AUTOGEN_CMD_MAX 100                /* The command line is a NULL terminated String array, Max number of Strings. */
#define N_PATTERNS 9                       /* Max number of file patterns. */
#define _CA_FLAG_FSIZE_NOT_REMEMBER (1<<1) /* The file size is normally saved in a hashmap and reused. */
#define _CA_FLAG_FSIZE_MULTIPLE (1<<2)     /* Guessed file size is    struct _autogen_config.estimated_filesize  multiplied with file size of input file */
#define _CA_FLAG_IGNORE_ERRFILE (1<<3)     /* Error EPIPE is normally returned if an error file exists from previously failed run. With this flag the file content is computed even if error file exist. */
#define _CA_FLAG_WITH_GENERATED_FILES_AS_INPUT_FILES (1<<4)  /* Experimental. Under construction */
enum _autogen_capture_output{STDOUT_DROP,STDOUT_TO_OUTFILE,STDOUT_TO_MALLOC,STDOUT_TO_MMAP,STDOUT_MERGE_TO_STDERR};
/* IMPORTANT:  Do not forget the terminal NULL in arrays of Strings !! */
struct _autogen_config{
  _AUTOGEN_CONFIG_INTERNAL_FIELDS;
  enum _autogen_capture_output out; /* Where should the Standard output of the called command go to */
  double estimated_filesize;        /* File size is guessed for not yet generated files. See  _CA_FLAG_FSIZE_MULTIPLE. */
  int concurrent_computations;      /* If 0 or 1 (recommended), it will be computed in a locked code block such that only one computation is performed at a time */
  bool no_redirect;                 /* Do not redirect the stdout / stderr. Was required for one Windows executable under wine. The process freezes when stdout st redirected. */
  int flags;                        /* Bitmask of bits  named  _CA_FLAG_.. Can be 0. */
  char *patterns[N_PATTERNS+1];     /* Filter files with literal strings (No regex!). The patterns[0], [1], [2], ... are AND-ed and the colon ":" separated within patterns[i] are OR-ed. */
  char *ends;                       /* Any ending must match. Can be NULL if treted in config_autogen_run().  */
  char *ends_ic;                    /* Same, ignore case. */
  const char *ext;                  /* Extension of generated file.  The extension is appended at the input file to form the out-file. */
  char *info;                       /* Will be printed into the logs. Can be NULL */
  char **env;                       /* Environment. See execle(). Can be NULL. */
  char *cmd[];                      /* Command line parameters.   Can be NULL. */
};


/* --- The following examples should be an inspiration and a template for own configurations --- */
#define IMG_SCALE_COMMON(op,out) .ends_ic=".jpeg:.jpg:.png:.gif",.info="Requires Imagemagick",.flags=_CA_FLAG_FSIZE_MULTIPLE,.estimated_filesize=1,.cmd={"convert",PLACEHOLDER_INFILE,"-scale",op,out,NULL}
#define INFO_TESSERACT .info="Requires  sudo apt-get install tesseract-ocr-eng"
struct _autogen_config
_scale25={.ext=".scale25%.",IMG_SCALE_COMMON("%25","-"),.out=STDOUT_TO_MMAP},
  _scale50={.ext=".scale50%.",IMG_SCALE_COMMON("%50",PLACEHOLDER_TMP_OUTFILE)},
  _opticalCharacterRecognition={INFO_TESSERACT, .ends_ic=".jpeg:.jpg:.png:.gif",.estimated_filesize=9999,.ext=".ocr.eng.txt",.out=STDOUT_TO_OUTFILE,.cmd={"tesseract",PLACEHOLDER_INFILE,"-","-l","eng",NULL}},
  _pdftotext={.info="Requires  poppler-utils", .ends_ic=".pdf",.estimated_filesize=9999,.ext=".txt",.cmd={"pdftotext",PLACEHOLDER_INFILE,PLACEHOLDER_TMP_OUTFILE,NULL}},
  _test_zip_checksums={.ends_ic=".zip",.estimated_filesize=9999,.ext=".txt",.cmd={"unzip","-t",PLACEHOLDER_INFILE,NULL},.out=STDOUT_TO_OUTFILE};

#undef IMG_SCALE_COMMON


/* --- Some test cases used for debugging. Create a test file 'test_autogen.txt' ... --- */
#define C(s)  .ends=".txt",.patterns={"test_autogen.txt",NULL},.estimated_filesize=99
struct _autogen_config
_test_malloc_fail= {  C(),.ext=".ls-fails.txt",     .out=STDOUT_TO_OUTFILE,.cmd={"ls","-l","not exist",NULL},},  /* Fails with  Operation not permitted */
  _test_cmd_notexist={C(),.ext=".cmd-not-exist.txt",.out=STDOUT_TO_MALLOC,.cmd={"not exist",NULL},},            /* Fails with  Broken Pipe */
  _test_malloc={      C(),.ext=".malloc.txt",       .out=STDOUT_TO_MALLOC,.cmd={"echo","Hello",NULL},},
  _test_mmap={        C(),.ext=".mmap.txt",         .out=STDOUT_TO_MMAP,.  cmd={"echo","Hello",NULL},},
  _test_textbuf={     C(),.ext=".textbuf.txt",                            .cmd={NULL}}; /* See config_autogen_run() */
#undef C


/* --- Configurations for mass spectrometry rawfiles --- */
struct _autogen_config
_wiff_strings={.ends=".wiff",.estimated_filesize=99999,.ext=".strings",.cmd={"bash","-c","tr -d '\\0' <"PLACEHOLDER_INFILE"|strings|grep '\\w\\w\\w\\w' # "PLACEHOLDER_TMP_OUTFILE" "PLACEHOLDER_TMP_OUTFILE,NULL}},
#define DOCKER_MSCONVERT_CMD "docker","run","-v",PLACEHOLDER_INFILE_PARENT":/data","-v",PLACEHOLDER_OUTFILE_PARENT":/dst","-it","--rm","chambm/pwiz-skyline-i-agree-to-the-vendor-licenses","wine","msconvert",PLACEHOLDER_INFILE_NAME,"--outdir","/dst", "--outfile",PLACEHOLDER_TMP_OUTFILE_NAME
#define _msconvert(extension,suffix,limit) {.info="Requires Docker",.ext=extension,.ends=suffix,.estimated_filesize=limit,.out=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT_CMD,NULL}}
  _msconvert_mzML=_msconvert(".mzML",".raw:.wiff",99999999999),
  _msconvert_mgf=_msconvert(".mgf",".raw:.wiff",99999999999),
  _wiff_scan={.ext=".scan",.ends=".wiff",.estimated_filesize=99999999999,.no_redirect=true,.cmd={"bash","rawfile_mk_wiff_scan.sh",PLACEHOLDER_INFILE,PLACEHOLDER_TMP_DIR,PLACEHOLDER_OUTFILE,NULL}};




////////////////////////////////////////////////
/// The list of all active configurations    ///
/// Comment those lines that you do not need ///
////////////////////////////////////////////////

static struct _autogen_config **config_autogen_rules(){
  static struct _autogen_config* aa[]={
    &_test_cmd_notexist,&_test_malloc_fail,
    &_test_textbuf, &_test_mmap, &_test_malloc,
    &_wiff_strings,
    &_wiff_scan,
    &_msconvert_mzML,
    &_msconvert_mgf,
    &_scale50,
    &_scale25,
    &_opticalCharacterRecognition,
    &_test_zip_checksums,
    &_pdftotext,
    NULL};
  return aa;
}
// see  FOREACH_AUTOGEN(i,s)

///////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Initially, there are tiny placeholder .wiff.scan files of 44 bytes  which do not hold information.  ///
/// Later they are replaced by larger .wiff.scan files                                                  ///
///////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool config_autogen_file_is_invalid(const char *path,const int path_l, struct stat *st, const char *rootpath){
  if (!st) return false;
  if (ENDSWITH(path,path_l,".wiff.scan") && st->st_size==44) return true; /*The tiny wiff.scan files are placeholders*/
  return  false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Before the file content is generated, its size can only be guessed.                                              ///
/// The estimated size must be at least the real size. Otherwise it may appear truncated when opened the first time. ///
///////////////////////////////////////////////////////////////////////////////////////////
static long config_autogen_estimated_filesize(const char *vp,const int vp_l, bool *cache_size){
  struct _autogen_config  *s=aimpl_struct_for_genfile(vp,vp_l);
  //log_entered_function("%s %p",vp,s);
  if (!s) return -1;
  *cache_size=!(s->flags&_CA_FLAG_FSIZE_NOT_REMEMBER);
  if (s->flags&_CA_FLAG_FSIZE_MULTIPLE){
    struct path_stat path_stat[AUTOGEN_MAX_DEPENDENCIES];
    long sum=0;
    RLOOP(i,autogen_find_sourcefiles(path_stat,vp,vp_l)) sum+=path_stat[i].stat.st_size;
    return sum*s->estimated_filesize;
  }
  return s->estimated_filesize;
}
static bool config_autogen_run(struct textbuffer *buf[], struct _autogen_config *s,  const char *outfile,const char *tmpoutfile,const char *logfile){ /*NOT_TO_HEADER*/
  if (s==&_test_textbuf){ /* This example shows how file content is generated without calling an external program. */
    textbuffer_add_segment((buf[0]=textbuffer_new(MALLOC_autogen_textbuffer)),strdup("How is it going?"),sizeof("How is it going?"));
    return true;
  }
  return false;
}
