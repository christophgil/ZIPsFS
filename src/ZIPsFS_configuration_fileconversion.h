/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
_Static_assert(WITH_FILECONVERSION,"");
#define FILECONVERSION_MMAP_MAX_BYTES (1<<30)                    /* For option STDOUT_TO_MMAP. If exceeded then create file as fall-back  */
#define FILECONVERSION_MALLOC_MAX_BYTES (1<<20)                  /* For option STDOUT_TO_MALLOC. If exceeded then create file as fall-back  */
#define FILECONVERSION_MAX_RULES 100                             /* Increase if there are more  returned by config_fileconversion_rules()  */
#define FILECONVERSION_ARGV 100                              /* The command line is a NULL terminated String array, Max number of Strings. */
#define FILECONVERSION_FILENAME_PATTERNS 9                       /* Max number of file patterns. */
#define DEFAULT_MIN_FREE_DISKCAPACITY_GB 100              /* Valid if min_free_diskcapacity_gb is unset */
#define DEFAULT_CONCURRENT_COMPUTATIONS_EXTERNAL_QUEUE 32 /* See PLACEHOLDER_EXTERNAL_QUEUE ZIPsFS_fileconversion_queue.sh */

#define FILECONVERSION_MAX_INFILES 5 /*  Dynamically generated file can depend on n input files.  Prevents runaway loop */

/* Unfortunately, Windows will not be able to open folder a when it is not shown in the parent file listing. */





/* The user can extend fileconversion_rule_t with additional fields: */
#define FILECONVERSION_RULE_CUSTOM_FIELDS  int this_is_my_field; int this_is_my_field_2


/* The user can add additional flags for  fileconversion_rule_t -> flags */
#define CA_FLAG_MY_FLAG       (1<<1)
#define CA_FLAG_MY_other_FLAG (1<<2)

#define FC_NOT_UPTODATE  (1<<1)
