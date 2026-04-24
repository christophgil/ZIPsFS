/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
_Static_assert(WITH_FILECONVERSION,"");



enum{
  FILECONVERSION_MMAP_MAX_BYTES=1024*1024*1024,                    /* For option STDOUT_TO_MMAP. If exceeded then create file as fall-back  */
  FILECONVERSION_MALLOC_MAX_BYTES=1024*1024,                  /* For option STDOUT_TO_MALLOC. If exceeded then create file as fall-back  */
  FILECONVERSION_MAX_RULES=100,                             /* Increase if there are more  returned by config_fileconversion_rules()  */
  FILECONVERSION_ARGV=100,             /* The command line is a NULL terminated String array, Max number of Strings. */
  FILECONVERSION_FILENAME_PATTERNS=9,                       /* Max number of file patterns. */
  DEFAULT_MIN_FREE_DISKCAPACITY_GB=100,              /* Valid if min_free_diskcapacity_gb is unset */
  DEFAULT_CONCURRENT_COMPUTATIONS_EXTERNAL_QUEUE=32, /* See PLACEHOLDER_EXTERNAL_QUEUE ZIPsFS_fileconversion_queue.sh */

  FILECONVERSION_MAX_INFILES=5, /*  Dynamically generated file can depend on n input files.  Prevents runaway loop */
};
/* Unfortunately, Windows will not be able to open folder a when it is not shown in the parent file listing. */





/* The user can extend struct fileconversion_rule with additional fields: */
#define FILECONVERSION_RULE_CUSTOM_FIELDS  int this_is_my_field; int this_is_my_field_2


/* The user can add additional flags for  struct fileconversion_rule -> flags */
enum {CA_FLAG_MY_FLAG=1<<1, CA_FLAG_MY_other_FLAG=1<<2};
enum {FC_NOT_UPTODATE=1<<1};
