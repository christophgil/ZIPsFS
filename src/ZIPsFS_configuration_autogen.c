/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
// cppcheck-suppress-file [constParameter,constParameterPointer]



/* --- The following examples should be an inspiration and a template for own configurations --- */
#define ENDS_IMG .ends_ic=".jpeg:.jpg:.png:.gif"
#define IMG_SCALE_COMMON(op,out) ENDS_IMG,.info="Needs Imagemagick",.flags=CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE|CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT,.estimated_filesize=1,.cmd={"convert",PLACEHOLDER_INFILE,"-scale",op,out,NULL}
#define INFO_TESSERACT .info="Requires  sudo apt-get install tesseract-ocr-eng"
struct autogen_rule
_scale25={.ext=".scale25%",IMG_SCALE_COMMON("%25","-"),.out=STDOUT_TO_MMAP},
  _scale50={.ext=".scale50%",IMG_SCALE_COMMON("%50",PLACEHOLDER_TMP_OUTFILE)},
  _opticalCharacterRecognition={INFO_TESSERACT, ENDS_IMG,.estimated_filesize=9999,.ext=".ocr.eng.txt",.out=STDOUT_TO_OUTFILE,.cmd={"tesseract",PLACEHOLDER_INFILE,"-","-l","eng",NULL}},
  _pdftotext={.info="Requires  poppler-utils", .ends=".pdf",.estimated_filesize=9999,.ext=".txt",.cmd={"pdftotext",PLACEHOLDER_INFILE,PLACEHOLDER_TMP_OUTFILE,NULL}},
  _test_zip_checksums={.ends_ic=".zip",.estimated_filesize=9999,.ext=".txt",.cmd={"unzip","-t",PLACEHOLDER_INFILE,NULL},.out=STDOUT_TO_OUTFILE},

  _bunzip2={.ends=".tsv.bz2",.estimated_filesize=9, .ext=".tsv",.cmd={"bunzip2","-c",PLACEHOLDER_INFILE,NULL},.out=STDOUT_TO_MMAP,
            .flags=CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE,.max_infilesize_for_RAM=(2<<20)},
  _parquet={.ends=".parquet",.estimated_filesize=30, .max_infilesize_for_RAM=(1<<20), .ext=".tsv", .out=STDOUT_TO_MMAP, .flags=CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE,
            .cmd={"docker","run","-v",PLACEHOLDER_INFILE_PARENT":/data","--rm","hangxie/parquet-tools","cat","-f","tsv","/data/"PLACEHOLDER_INFILE_NAME,NULL}},

/* Beware circular conversions therefore upper case .TSV.bz2 */
  _parquet_bz2={.ends=".parquet",.estimated_filesize=10, .ext=".TSV.bz2", .out=STDOUT_TO_OUTFILE, .flags=CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE|CA_FLAG_SECURITY_CHECK_FILENAME,
                .cmd={"bash","-c", "docker run -v '"PLACEHOLDER_INFILE_PARENT":/data' --rm hangxie/parquet-tools cat -f tsv '/data/"PLACEHOLDER_INFILE_NAME"' |bzip2 -c",NULL}},
  _autogen_rule_null={};
#undef IMG_SCALE_COMMON

/* --- Some test cases used for debugging. Create a test file 'test_autogen.txt' ... --- */
#define C(s)  .ends=".txt",.patterns={"test_autogen",NULL},.estimated_filesize=99
struct autogen_rule
_test_malloc_fail={    C(),.ext=".ls-fails.txt",     .out=STDOUT_TO_OUTFILE,.cmd={"ls","-l","not exist",NULL},},  /* Fails with  Operation not permitted */
  _test_cmd_notexist={ C(),.ext=".cmd-not-exist.txt",.out=STDOUT_TO_MALLOC,.cmd={"not exist",NULL},},            /* Fails with  Broken Pipe */
  _test_malloc={       C(),.ext=".malloc.txt",       .out=STDOUT_TO_MALLOC,.cmd={"echo","Hello",NULL},},
  _test_mmap={         C(),.ext=".mmap.txt",         .out=STDOUT_TO_MMAP,.  cmd={"echo","Hello",NULL},},
  _test_noext={        C(),.ext=".mmap.noext.TXT",   .out=STDOUT_TO_MMAP,.  cmd={"echo","Hello",NULL},.flags=CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE,},
  _test_textbuf={      C(),.ext=".textbuf.txt",                            .cmd={NULL}}; /* See config_autogen_run() */
#undef C


/* --- Configurations for mass spectrometry rawfiles --- */
struct autogen_rule
//_wiff_strings={.ends=".wiff",.estimated_filesize=99999,.ext=".strings",.cmd={"bash","-c","tr -d '\\0' <"PLACEHOLDER_INFILE"|strings|grep '\\w\\w\\w\\w' # "PLACEHOLDER_TMP_OUTFILE" "PLACEHOLDER_TMP_OUTFILE,NULL}},
_wiff_strings={.ends=".wiff",.estimated_filesize=99999,.ext=".strings",.cmd={"bash","-c","tr -d '\\0' <'"PLACEHOLDER_INFILE"'|strings|grep '\\w\\w\\w\\w'  ", NULL}, .out=STDOUT_TO_MALLOC,.flags=CA_FLAG_SECURITY_CHECK_FILENAME},
#define DOCKER_MSCONVERT_CMD "docker","run","-v",PLACEHOLDER_INFILE_PARENT":/data","-v",PLACEHOLDER_OUTFILE_PARENT":/dst","-it","--rm","chambm/pwiz-skyline-i-agree-to-the-vendor-licenses","wine","msconvert",PLACEHOLDER_INFILE_NAME,"--outdir","/dst", "--outfile",PLACEHOLDER_TMP_OUTFILE_NAME
#define _msconvert(extension,suffix,limit) {.info="Requires Docker",.ext=extension,.ends=suffix,.estimated_filesize=limit,.out=STDOUT_TO_OUTFILE,.cmd={DOCKER_MSCONVERT_CMD,NULL}}
  _msconvert_mzML=_msconvert(".mzML",".raw:.wiff",99999999999),
  _msconvert_mgf=_msconvert(".mgf",".raw:.wiff",99999999999),
  _wiff_scan={.ext=".scan",.ends=".wiff",.estimated_filesize=99999999999,.no_redirect=true,.cmd={"bash","rawfile_mk_wiff_scan.sh",PLACEHOLDER_INFILE,PLACEHOLDER_TMP_DIR,PLACEHOLDER_OUTFILE,NULL}};

////////////////////////////////////////////////
/// The list of all active configurations    ///
/// Comment those lines that you do not need ///
////////////////////////////////////////////////

static struct autogen_rule **config_autogen_rules(void){
  static struct autogen_rule* aa[]={
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
    &_parquet,
    &_parquet_bz2,
    /* --- At the end those with CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT --- */
    /* --- This flag can cause unambiguity unambiguous assignment of generated file to autogen_rule.  Best is to put those configurations at the end of the list --- */
    &_test_noext,
    //   &_bunzip2,
    NULL,NULL
    }; /* The terminal NULL is required !! */

  return aa;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Initially, there are tiny placeholder .wiff.scan files of 44 bytes  which do not hold information.  ///
/// Later they are replaced by larger .wiff.scan files                                                  ///
///////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool config_autogen_file_is_invalid(const char *path,const int path_l, const struct stat *st, const char *rootpath){
  if (!st) return false;
  if (ENDSWITH(path,path_l,".wiff.scan") && st->st_size==44) return true; /* Sciex Mass-Spectrometer: The tiny wiff.scan files are placeholders*/
  return  false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Before the file content is generated, its size can only be guessed.                                              ///
/// The estimated size must be at least the real size. Otherwise it may appear truncated when opened the first time. ///
/// Return: -1  Not processed                                                                                        ///
///          0  Was processed successfully                                                                           ///
///          Errorcode like ENOENT in case of error                                                                  ///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static long config_autogen_estimate_filesize(const struct autogen_files *ff, bool *rememberFileSize){
  if (ff->rule->flags&CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE){
    if (0==(ff->rule->flags&CA_FLAG_FSIZE_NOT_REMEMBER)) *rememberFileSize=true;
    return ff->infiles_size_sum*ff->rule->estimated_filesize;
  }
  return ff->rule->estimated_filesize;
}

/////////////////////////////////////////////////////////////////////////
/// The argv for  execl()  could be modified here.                   ///
/////////////////////////////////////////////////////////////////////////
static void *config_autogen_modify_exec_args(char *cmd[AUTOGEN_ARGV_MAX+1],const struct autogen_files *ff){
  return cmd;
}
////////////////////////////////////////////////////////////////////////////////////////////
/// Typically, a generated file depends on one input file ff->vinfiles[0]                ///
/// If there are further input files then set virtual paths which need to be on the heap ///
/// ff->vinfiles[1]=..., ff->vinfiles[2]=...                                             ///
/// Return total number                                                                  ///
////////////////////////////////////////////////////////////////////////////////////////////
static int config_autogen_add_virtual_infiles(struct autogen_files *ff){
  return 1;
}

static enum autogen_run_res config_autogen_run(struct autogen_files *ff){
  if (ff->rule==&_test_textbuf){ /* This example shows how file content is generated without calling an external program. */
    /* There are three functions to add text:
       autogen_filecontent_append(): The memory had been allocated with malloc() and will be released with free()
       autogen_filecontent_append_nodestroy(): The memory will not be released. For example string constant.
       autogen_filecontent_append_munmap(): The memory had been obtained with malloc() and will be freed with munmap()
    */
    const char *t;
    t="This is a string literal and must not be released with free().\n";
    if (autogen_filecontent_append_nodestroy(ff,t,strlen(t))) return AUTOGEN_RUN_FAIL;
    t="This text is in the heap storage. It should be released with free().\n";
    if (autogen_filecontent_append(ff,strdup(t),strlen(t))) return AUTOGEN_RUN_FAIL;
    return AUTOGEN_RUN_SUCCESS;
  }
  return AUTOGEN_RUN_NOT_APPLIED;
}
