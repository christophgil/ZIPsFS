/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////

#define AUTOGEN_ARGV_MAX 100                /* The command line is a NULL terminated String array, Max number of Strings. */
#define N_PATTERNS 9                       /* Max number of file patterns. */
#define CA_FLAG_FSIZE_NOT_REMEMBER (1<<1) /* The file size is normally saved in a hashmap and reused. */
#define CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE (1<<2)     /* Guessed file size is    struct autogen_config.estimated_filesize  multiplied with file size of input file */
#define CA_FLAG_IGNORE_ERRFILE (1<<3)     /* Error EPIPE is normally returned if an error file (.fail.txt)  exists from previous failure. With this flag the file content is computed even if outputfile.fail.txt exists. */
#define CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT   (1<<4) /* Strip file ext from the infile and append the ext for the output file. */
#define CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT   (1<<5) /* Strip file ext from the infile and append the ext for the output file. */

/* This flag can cause unambiguity unambiguous assignment of generated file to autogen_config.  Best is to put those configurations at the end of the list */
#define DEFAULT_MIN_FREE_DISKCAPACITY_GB 100  /* Valid if min_free_diskcapacity_gb is unset */
#define DEFAULT_CONCURRENT_COMPUTATIONS_EXTERNAL_QUEUE 32 /* See PLACEHOLDER_EXTERNAL_QUEUE ZIPsFS_autogen_queue.sh */
#define CA_FLAG_WITH_GENERATED_FILES_AS_INPUT_FILES (1<<4)  /* Experimental. Under construction */
enum _autogen_capture_output{STDOUT_DROP,              /* Ignore standard output stream of the external app */
                             STDOUT_TO_OUTFILE,        /* Save the standard output stream of the external app in the output file */
                             STDOUT_TO_MALLOC,         /* Temporarily keep the standard output stream of the external app in the RAM until the file pointer is closed. */
                             STDOUT_TO_MMAP,           /* Same.  STDOUT_TO_MALLOC uses the application heap which has a limited size. Use STDOUT_TO_MALLOC for larger outputs. */
                             STDOUT_MERGE_WITH_STDERR};/* Both  output streams of the external app go to the outputfile-dot-log  file. */

/* IMPORTANT:  Do not forget the terminal NULL in arrays of Strings !! */
struct autogen_config{
  _AUTOGEN_CONFIG_INTERNAL_FIELDS;
  enum _autogen_capture_output out; /* Where should the Standard output of the called command go to */
  double estimated_filesize;        /* File size is guessed for not yet generated files. See  CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE. */
  int concurrent_computations;      /* If 0 or 1 (recommended), it will be computed in a locked code block such that only one computation is performed at a time */
  int min_free_diskcapacity_gb;     /* If the free disk  capacity is below, no files are created. Error ENOSPC is returned. If zero, DEFAULT_MIN_FREE_DISKCAPACITY_GB is used. */
  int max_infilesize_for_stdout_in_RAM; /* If not 0 and if exceeded by size of infile, then treat like .out=STDOUT_TO_OUTFILE */
  bool no_redirect;                 /* Do not redirect the stdout / stderr. Was required for one Windows executable under wine. The process freezes when stdout st redirected. */
  int flags;                        /* Bitmask of bits  named  CA_FLAG_.. Can be 0. */
  char *patterns[N_PATTERNS+1];     /* Filter files with literal strings (No regex!). The patterns[0], [1], [2], ... are AND-ed and the colon ":" separated within patterns[i] are OR-ed.*/
  char *ends;                       /* Any ending must match. Can be NULL if treted in config_autogen_run().  */
  char *ends_ic;                    /* Same, ignore case. */
  const char *ext;                  /* Extension of generated file.  The extension is appended at the input file to form the out-file. */
  char *info;                       /* Will be printed into the logs. Can be NULL */
  char **env;                       /* Environment. See execle(). Can be NULL. */
  char *cmd[];                      /* Command line parameters.   Can be NULL. */
};


/* --- The following examples should be an inspiration and a template for own configurations --- */
#define IMG_SCALE_COMMON(op,out) .ends_ic=".jpeg:.jpg:.png:.gif",.info="Requires Imagemagick",.flags=CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE|CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT,.estimated_filesize=1,.cmd={"convert",PLACEHOLDER_INFILE,"-scale",op,out,NULL}
#define INFO_TESSERACT .info="Requires  sudo apt-get install tesseract-ocr-eng"
struct autogen_config
_scale25={.ext=".scale25%",IMG_SCALE_COMMON("%25","-"),.out=STDOUT_TO_MMAP},
  _scale50={.ext=".scale50%",IMG_SCALE_COMMON("%50",PLACEHOLDER_TMP_OUTFILE)},
  _opticalCharacterRecognition={INFO_TESSERACT, .ends_ic=".jpeg:.jpg:.png:.gif",.estimated_filesize=9999,.ext=".ocr.eng.txt",.out=STDOUT_TO_OUTFILE,.cmd={"tesseract",PLACEHOLDER_INFILE,"-","-l","eng",NULL}},
  _pdftotext={.info="Requires  poppler-utils", .ends=".pdf",.estimated_filesize=9999,.ext=".txt",.cmd={"pdftotext",PLACEHOLDER_INFILE,PLACEHOLDER_TMP_OUTFILE,NULL}},
  _test_zip_checksums={.ends_ic=".zip",.estimated_filesize=9999,.ext=".txt",.cmd={"unzip","-t",PLACEHOLDER_INFILE,NULL},.out=STDOUT_TO_OUTFILE},
  _bunzip2={.ends=".tsv.bz2",.estimated_filesize=9, .ext=".tsv",.cmd={"bunzip2","-c",PLACEHOLDER_INFILE,NULL},.out=STDOUT_TO_MMAP, .flags=CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE,
            .max_infilesize_for_stdout_in_RAM=(2<<10)};

#undef IMG_SCALE_COMMON


/* --- Some test cases used for debugging. Create a test file 'test_autogen.txt' ... --- */
#define C(s)  .ends=".txt",.patterns={"test_autogen",NULL},.estimated_filesize=99
struct autogen_config
_test_malloc_fail= {  C(),.ext=".ls-fails.txt",     .out=STDOUT_TO_OUTFILE,.cmd={"ls","-l","not exist",NULL},},  /* Fails with  Operation not permitted */
  _test_cmd_notexist={C(),.ext=".cmd-not-exist.txt",.out=STDOUT_TO_MALLOC,.cmd={"not exist",NULL},},            /* Fails with  Broken Pipe */
  _test_malloc={      C(),.ext=".malloc.txt",       .out=STDOUT_TO_MALLOC,.cmd={"echo","Hello",NULL},},
  _test_mmap={        C(),.ext=".mmap.txt",         .out=STDOUT_TO_MMAP,.  cmd={"echo","Hello",NULL},},
  _test_noext={       C(),.ext=".mmap.noext.txt",   .out=STDOUT_TO_MMAP,.  cmd={"echo","Hello",NULL},.flags=CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE,},
  _test_textbuf={     C(),.ext=".textbuf.txt",                            .cmd={NULL}}; /* See config_autogen_run() */
#undef C


/* --- Configurations for mass spectrometry rawfiles --- */
struct autogen_config
_wiff_strings={.ends=".wiff",.estimated_filesize=99999,.ext=".strings",.cmd={"bash","-c","tr -d '\\0' <"PLACEHOLDER_INFILE"|strings|grep '\\w\\w\\w\\w' # "PLACEHOLDER_TMP_OUTFILE" "PLACEHOLDER_TMP_OUTFILE,NULL}},
#define DOCKER_MSCONVERT_CMD "docker","run","-v",PLACEHOLDER_INFILE_PARENT":/data","-v",PLACEHOLDER_OUTFILE_PARENT":/dst","-it","--rm","chambm/pwiz-skyline-i-agree-to-the-vendor-licenses","wine","msconvert",PLACEHOLDER_INFILE_NAME,"--outdir","/dst", "--outfile",PLACEHOLDER_TMP_OUTFILE_NAME
#define _msconvert(extension,suffix,limit) {.info="Requires Docker",.ext=extension,.ends=suffix,.estimated_filesize=limit,.out=STDOUT_MERGE_WITH_STDERR,.cmd={DOCKER_MSCONVERT_CMD,NULL}}
  _msconvert_mzML=_msconvert(".mzML",".raw:.wiff",99999999999),
  _msconvert_mgf=_msconvert(".mgf",".raw:.wiff",99999999999),
  _wiff_scan={.ext=".scan",.ends=".wiff",.estimated_filesize=99999999999,.no_redirect=true,.cmd={"bash","rawfile_mk_wiff_scan.sh",PLACEHOLDER_INFILE,PLACEHOLDER_TMP_DIR,PLACEHOLDER_OUTFILE,NULL}};




////////////////////////////////////////////////
/// The list of all active configurations    ///
/// Comment those lines that you do not need ///
////////////////////////////////////////////////

static struct autogen_config **config_autogen_rules(){
  static struct autogen_config* aa[]={
    &_pdftotext,

    &_test_mmap,
    &_test_textbuf,
    &_test_cmd_notexist,&_test_malloc_fail,
    &_test_malloc,
    &_wiff_strings,
    &_wiff_scan,
    &_msconvert_mzML,
    &_msconvert_mgf,
    &_scale50,
    &_scale25,
    &_opticalCharacterRecognition,
    &_test_zip_checksums,
    // --- At the end those with CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT ---
    &_test_noext,
    &_bunzip2,
    NULL}; /* The terminal NULL is required !! */
  return aa;
}
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
/// Return: -1  Not processed                                                                                        ///
///          0  Was processed successfully                                                                           ///
///          Errorcode like ENOENT in case of error                                                                  ///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static long config_autogen_estimate_filesize(const char *vp,const int vp_l, bool *cache_size){
  struct autogen_config  *s=autogen_for_vgenfile(NULL,vp,vp_l);
  //bool debug=strstr(vp,"img_2.scale")  && s && (s->flags&CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT);
  if (!s) return -1;
  *cache_size=!(s->flags&CA_FLAG_FSIZE_NOT_REMEMBER);
  if (s->flags&CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE){
    long sum=0;
    struct stat stats[AUTOGEN_MAX_INFILES+1];
    RLOOP(i,autogen_rinfiles_for_vgenfile(NULL,stats,vp,false))  sum+=stats[i].st_size;
    return sum*s->estimated_filesize;
  }
  return s->estimated_filesize;
}

/////////////////////////////////////////////////////////////////////////
/// The argv for  execl()  could be modified here                     ///
/////////////////////////////////////////////////////////////////////////
static void config_autogen_modify_exec_args(char *cmd[],struct autogen_files *ff,const struct autogen_config *s){
}

//////////////////////////////////////////////////////////////////////////////////////////
/// Provide further virtual input file paths for a given virtual generated file path   ///
/// if a generated file depends on two or more input files.                            ///
/// infile[0] is already set.                                                          ///
/// Return the number of infiles, normally 1.  Maximum is AUTOGEN_MAX_DEPENDENCIES.    ///
//////////////////////////////////////////////////////////////////////////////////////////
static int config_autogen_vinfiles_for_vgenfile(char vinfiles[AUTOGEN_MAX_INFILES+1][MAX_PATHLEN+1],const char *generated,const int generated_l, struct autogen_config *s){
  return 1;
}

static int config_autogen_run(const struct autogen_config *s,  struct autogen_files *ff){ /*NOT_TO_HEADER*/
  if (s==&_test_textbuf){ /* This example shows how file content is generated without calling an external program. */
    /* There are three functions to add text:
       autogen_filecontent_append(): The memory had been allocated with malloc() and will be released with free()
       autogen_filecontent_append_nodestroy(): The memory will not be released. For example string constant.
       autogen_filecontent_append_munmap(): The memory had been obtained with malloc() and will be freed with munmap()
    */
    char *s;
    s="This is a string literal and must not be released with free().\n"; autogen_filecontent_append_nodestroy(ff,s,strlen(s));       /* Will not be  destroyed */
    s="This text is in the heap storage. It should be released with free().\n";  autogen_filecontent_append(ff,strdup(s),strlen(s)); /* Will be destroyed with free() */
    return 0;
  }
  return -1;
}
