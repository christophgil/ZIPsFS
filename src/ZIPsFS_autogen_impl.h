/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////

#define FOREACH_AUTOGEN(i,s)  struct _autogen_config *s; for(int i=0;(s=config_autogen_rules()[i]);i++)
#define _AUTOGEN_CONFIG_INTERNAL_FIELDS\
  char **_patterns[N_PATTERNS];int *_patterns_l[N_PATTERNS];\
  char **_ends;int *_ends_l;\
  char **_ends_ic;int *_ends_ic_l;\
  int _ext_l;\
  atomic_int _count_concurrent;
