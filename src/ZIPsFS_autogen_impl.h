/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////




#define FOREACH_AUTOGEN_RULE(i,ac)  struct autogen_rule *ac; for(int i=0;(ac=config_autogen_rules()[i]);i++)
//#define FOREACH_AUTOGEN_RULE(ac)  for(const struct autogen_rule *ac=*config_autogen_rules(); ac&&ac->_idx; ac++)
#define autogen_filecontent_append_nodestroy(ff,s,s_l)  _autogen_filecontent_append(TEXTBUFFER_NODESTROY,ff,s,s_l)
#define autogen_filecontent_append(ff,s,s_l)  _autogen_filecontent_append(0,ff,s,s_l)
#define autogen_filecontent_append_munmap(ff,s,s_l)  _autogen_filecontent_append(TEXTBUFFER_MUNMAP,ff,s,s_l)


#define CA_FLAG_FSIZE_NOT_REMEMBER                  (1<<25) /* The file size is normally saved in a hashmap and reused. */
#define CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE         (1<<26) /* Guessed file size is    struct autogen_rule.estimated_filesize  multiplied with file size of input file */
#define CA_FLAG_IGNORE_ERRFILE                      (1<<27) /* Ignore outputfile.fail.txt  from previous failure and do not return EPIPE. Instead try computation again. */
#define CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT    (1<<28) /* Strip file ext from the infile and append the ext for the output file. */
#define CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT  (1<<29) /* Strip file ext from the infile and append the ext for the output file. */
#define CA_FLAG_SECURITY_CHECK_FILENAME             (1<<30) /* Prevent code injection by funny file names */
#define CA_FLAG_WITH_GENERATED_FILES_AS_INPUT_FILES (1<<31) /* Experimental. Under construction */


enum _autogen_capture_output{STDOUT_DROP,              /* Ignore standard output stream of the external app */
                             STDOUT_TO_OUTFILE,        /* Save the standard output stream of the external app in the output file */
                             STDOUT_TO_MALLOC,         /* Temporarily keep the standard output stream of the external app in the RAM until the file pointer is closed. */
                             STDOUT_TO_MMAP,           /* Same.  STDOUT_TO_MALLOC uses the application heap which has a limited size. Use STDOUT_TO_MALLOC for larger outputs. */
                             STDOUT_MERGE_WITH_STDERR};/* Both  output streams of the external app go to the outputfile-dot-log  file. */



/* IMPORTANT:  Do not forget the terminal NULL in arrays of Strings !! */
struct autogen_rule{
  AUTOGEN_RULE_CUSTOM_FIELDS;
  /* internal start fields start with underscore */
  int _seqnum;                            /* 0, 1, 2, 3 ... */
  const char **_patterns[AUTOGEN_FILENAME_PATTERNS];     /* ->patterns  splitted at space */
  int *_patterns_l[AUTOGEN_FILENAME_PATTERNS];           /* Number of ->patterns  splitted at space */
  const char **_ends, **_ends_ic;         /* ends splitted at space */
  int *_ends_ll, *_ends_ic_ll;            /* String lengths */
  int _ext_l;
  /* -------------------------------------------- */
  enum _autogen_capture_output out;   /* Where should the Standard output of the called command go to */
  double estimated_filesize;          /* File size is guessed for not yet generated files. See  CA_FLAG_FSIZE_IS_MULTIPLE_OF_INFILE. */
  int concurrent_computations;        /* If 0 or 1 (recommended), it will be computed in a locked code block such that only one computation is performed at a time */
  int min_free_diskcapacity_gb;       /* If the free disk  capacity is below, no files are created. Error ENOSPC is returned. If zero, DEFAULT_MIN_FREE_DISKCAPACITY_GB is used. */
  int max_infilesize_for_RAM;         /* If not 0 and if exceeded by size of infile, then treat like .out=STDOUT_TO_OUTFILE */
  bool no_redirect;                   /* Do not redirect the stdout / stderr. Was required for one Windows executable under wine. The process freezes when stdout st redirected. */
  int flags;                          /* Bitmask of bits  named  CA_FLAG_.. Can be 0. */
  const char *patterns[AUTOGEN_FILENAME_PATTERNS+1]; /* Filter files with literal strings (No regex!). The patterns[0], [1], [2], ... are AND-ed and the colon ":" separated within patterns[i] are OR-ed.*/
  const char *ends;                   /* Any ending must match. Can be NULL if treted in config_autogen_run().  */
  const char *ends_ic;                /* Same, ignore case. */
  const char *ext;                    /* Extension of generated file.  The extension is appended at the input file to form the out-file. */
  const char *info;                         /* Will be printed into the logs. Can be NULL */
  const char **env;                         /* Environment. See execle(). Can be NULL. */
  const char *cmd[];                        /* Command line parameters.   Can be NULL. */

};

struct autogen_files{
  const struct autogen_rule *rule;
  char virtualpath[MAX_PATHLEN+1];
  int virtualpath_l;
  char vinfiles[AUTOGEN_MAX_INFILES+1][MAX_PATHLEN+1];
  char rinfiles[AUTOGEN_MAX_INFILES+1][MAX_PATHLEN+1];
  struct stat infiles_stat[AUTOGEN_MAX_INFILES];
  long infiles_size_sum;
  int infiles_n;
  const char *grealpath;
  char tmpout[MAX_PATHLEN+1];
  char log[MAX_PATHLEN+1];
  char fail[MAX_PATHLEN+1];
  struct textbuffer *buf;
    enum _autogen_capture_output out;   /* Like autogen_rule.out.  Corrected for low memory */

};


enum autogen_run_res{
  AUTOGEN_RUN_SUCCESS,
  AUTOGEN_RUN_FAIL,
  AUTOGEN_RUN_NOT_APPLIED
};
