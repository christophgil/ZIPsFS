/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
_Static_assert(WITH_FILECONVERSION,"");



#define FOREACH_FILECONVERSION_RULE(i,ac)  struct fileconversion_rule *ac; for(int i=0;(ac=config_fileconversion_rules()[i]);i++)
//#define FOREACH_FILECONVERSION_RULE(ac)  for(const struct fileconversion_rule *ac=*config_fileconversion_rules(); ac&&ac->_idx; ac++)
#define fileconversion_filecontent_append_nodestroy(ff,s,s_l)  _fileconversion_filecontent_append(TXTBUFSGMT_NO_FREE,ff,s,s_l)
#define fileconversion_filecontent_append(ff,s,s_l)  _fileconversion_filecontent_append(0,ff,s,s_l)
#define fileconversion_filecontent_append_munmap(ff,s,s_l)  _fileconversion_filecontent_append(TXTBUFSGMT_MUNMAP,ff,s,s_l)




enum enum_fileconversion_capture_output{STDOUT_DROP,              /* Ignore standard output stream of the external app */
                             STDOUT_TO_OUTFILE,        /* Save the standard output stream of the external app in the output file */
                             STDOUT_TO_MALLOC,         /* Temporarily keep the standard output stream of the external app in the RAM until the file pointer is closed. */
                             STDOUT_TO_MMAP,           /* Same.  STDOUT_TO_MALLOC uses the application heap which has a limited size. Use STDOUT_TO_MALLOC for larger outputs. */
                             STDOUT_MERGE_WITH_STDERR};/* Both  output streams of the external app go to the outputfile-dot-log  file. */

struct fileconversion_rule{
  FILECONVERSION_RULE_CUSTOM_FIELDS;
  /* internal start fields start with underscore */
  int _seqnum;                            /* 0, 1, 2, 3 ... */
  const char **_patterns[FILECONVERSION_FILENAME_PATTERNS+1];     /* From patterns  splitted at colon */
  int *_patterns_l[FILECONVERSION_FILENAME_PATTERNS+1];           /* Number of ->patterns  splitted at colon */
  const char **_xpatterns[FILECONVERSION_FILENAME_PATTERNS+1];     /* From exclude_patterns  splitted at colon */
  int *_xpatterns_l[FILECONVERSION_FILENAME_PATTERNS+1];

  const char **_ends, **_ends_ic;         /* ends splitted at space */
  int *_ends_ll, *_ends_ic_ll;            /* String lengths */
  int _ext_l;
  /* -------------------------------------------- */
  enum enum_fileconversion_capture_output out;   /* Where should the Standard output of the called command go to */
  double estimated_filesize;          /* File size is guessed for not yet generated files. See  CA_FLAG_fsize_is_multiple_of_infile. */
  int concurrent_computations;        /* If 0 or 1 (recommended), it will be computed in a locked code block such that only one computation is performed at a time */
  int min_free_diskcapacity_gb;       /* If the free disk  capacity is below, no files are created. Error ENOSPC is returned. If zero, DEFAULT_MIN_FREE_DISKCAPACITY_GB is used. */
  int max_infilesize_for_RAM;         /* If not 0 and if exceeded by size of infile, then treat like .out=STDOUT_TO_OUTFILE */
  bool no_redirect;                   /* Do not redirect the stdout / stderr. Was required for one Windows executable under wine. The process freezes when stdout st redirected. */
  const char *patterns[FILECONVERSION_FILENAME_PATTERNS], *exclude_patterns[FILECONVERSION_FILENAME_PATTERNS];
  /* Filter files with literal strings (No regex!). At least one of  patterns[0], [1], [2], must match.
     Each string can contain several patterns separated by colon ":" which must all match (logical AND).*/
  const char *ends;                   /* Any ending must match. Omitted if processed in config_fileconversion_run().  */
  const char *ends_ic;                /* Same, ignore case. */
  const char *ext;                    /* Extension of generated file.  The extension is appended at the input file to form the out-file. */
  const char *info;                   /* Will be printed into the logs. Optional. */
  const char **env;                   /* Environment. Optional NULL terminated string array like .env={MY_PATH="/local/app/lib",NULL}. See execle(). */
  const char *cmd[FILECONVERSION_ARGV];      /* Command line parameters. */

  bool fsize_not_remember;                  /* The file size is normally saved in a hashmap and reused. */
  bool fsize_is_multiple_of_infile;         /* Guessed file size is    struct fileconversion_rule.estimated_filesize  multiplied with file size of input file */
  bool ignore_errfile;                      /* Ignore outputfile.fail.txt  from previous failure and do not return EPIPE. Instead try computation again. */
  bool generated_file_hasnot_infile_ext;    /* Strip file ext from the infile and append the ext for the output file. */
  bool generated_file_inherits_infile_ext;  /* Strip file ext from the infile and append the ext for the output file. */
  bool security_check_filename;             /* Prevent code injection by funny file names */
};

struct fileconversion_files{
  const struct fileconversion_rule *rule;
  char virtualpath[MAX_PATHLEN+1];
  int virtualpath_l;
  char vinfiles[FILECONVERSION_MAX_INFILES+1][MAX_PATHLEN+1];
  char rinfiles[FILECONVERSION_MAX_INFILES+1][MAX_PATHLEN+1];
  struct stat infiles_stat[FILECONVERSION_MAX_INFILES];
  long infiles_size_sum;
  int infiles_n;
  const char *grealpath;
  char tmpout[MAX_PATHLEN+1];
  char log[MAX_PATHLEN+1];
  char fail[MAX_PATHLEN+1];
  textbuffer_t *af_txtbuf;
    enum enum_fileconversion_capture_output out;   /* Like fileconversion_rule.out.  Corrected for low memory */

};


enum enum_fileconversion_run_res{ FILECONVERSION_RUN_SUCCESS,FILECONVERSION_RUN_FAIL,FILECONVERSION_RUN_NOT_APPLIED};
