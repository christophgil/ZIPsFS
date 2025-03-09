/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
#define AUTOGEN_MMAP_MAX_BYTES (1<<30)                    /* For option STDOUT_TO_MMAP. If exceeded then create file as fall-back  */
#define AUTOGEN_MALLOC_MAX_BYTES (1<<20)                  /* For option STDOUT_TO_MALLOC. If exceeded then create file as fall-back  */
#define AUTOGEN_MAX_RULES 100                             /* Increase if there are more  returned by config_autogen_rules()  */
#define AUTOGEN_ARGV_MAX 100                              /* The command line is a NULL terminated String array, Max number of Strings. */
#define AUTOGEN_FILENAME_PATTERNS 9                       /* Max number of file patterns. */
#define DEFAULT_MIN_FREE_DISKCAPACITY_GB 100              /* Valid if min_free_diskcapacity_gb is unset */
#define DEFAULT_CONCURRENT_COMPUTATIONS_EXTERNAL_QUEUE 32 /* See PLACEHOLDER_EXTERNAL_QUEUE ZIPsFS_autogen_queue.sh */

#define AUTOGEN_MAX_INFILES 5 /*  Dynamically generated file can depend on n input files.  Prevents runaway loop */

#define WITH_AUTOGEN_DIR_HIDDEN 0  /* Hide the directory a in ZIPsFS to avoid recursive searches. */
/* Unfortunately, Windows will not be able to open folder a when it is not shown in the parent file listing. */
#define AUTOGEN_DELETE_FILES_AFTER_DAYS "99"




/* The user can extend struct autogen_rule with additional fields: */
#define AUTOGEN_RULE_CUSTOM_FIELDS  int this_is_my_field; int this_is_my_field_2


/* The user can add additional flags for  struct autogen_rule -> flags */
#define CA_FLAG_MY_FLAG       (1<<1)
#define CA_FLAG_MY_other_FLAG (1<<2)
