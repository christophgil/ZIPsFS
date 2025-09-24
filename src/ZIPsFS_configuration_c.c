/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
// See ZIPsFS_c.c  WITH_CCODE
#define ZIPSFS_C_MAX_NUM 10 /* Maximum number of synthetic file definitions */
#define CONFIG_C_EXAMPLE_DIR "example_generated_file"
#define CONFIG_C_EXAMPLE     "example_generated_file.txt"
#define CONFIG_C_EXAMPLE_DIR_L (sizeof(CONFIG_C_EXAMPLE_DIR)-1)
#define CONFIG_C_EXAMPLE_L     (sizeof(CONFIG_C_EXAMPLE)-1)
#define CONFIG_C_IS_EXAMPLE(vp,vp_l)  (vp_l==1+CONFIG_C_EXAMPLE_DIR_L+1+CONFIG_C_EXAMPLE_L && vp[1+CONFIG_C_EXAMPLE_DIR_L]=='/' && !strcmp(vp+1,CONFIG_C_EXAMPLE_DIR"/"CONFIG_C_EXAMPLE))

static bool config_c_open(const int flags, const char *vp, const int vp_l){
  if (CONFIG_C_IS_EXAMPLE(vp,vp_l)) return true;
  return false;
}
static bool config_c_getattr(const int flags, const char *vp, const int vp_l, struct stat *st){
  if (vp_l-1==CONFIG_C_EXAMPLE_DIR_L && !strcmp(vp+1,CONFIG_C_EXAMPLE_DIR)){
    st->st_mode|=S_IFDIR;
    return true;
  }
  if (CONFIG_C_IS_EXAMPLE(vp,vp_l)){
    FSIZE_FROM_HASHTABLE(st,vp,vp_l, 999);
    st->st_mtime=time(NULL);
    return true;
  }
  return false;
}

static bool config_c_readdir(const int flags,const char *vp, const int vp_l,const int iTh, char *name, const int path_max,bool *isDirectory){
  assert(iTh<ZIPSFS_C_MAX_NUM);
  //log_debug_now("%d) vp: %s",iTh,vp);
  const char *n=NULL;
  switch(iTh){
  case 0:
    if (!vp_l){
      n=CONFIG_C_EXAMPLE_DIR;
      *isDirectory=true;
    }
    break;
  case 1:
    if (vp_l==CONFIG_C_EXAMPLE_DIR_L+1 && !strcmp(vp+1,CONFIG_C_EXAMPLE_DIR)) n=CONFIG_C_EXAMPLE;
    break;
  default: return false;
  }
  if (n)strcpy(name,n);
  return true;
}




static bool config_c_read(c_read_handle_t handle,const int flags,const char *vp, const int vp_l){
  if (CONFIG_C_IS_EXAMPLE(vp,vp_l)){
    const char *src=__FILE__;
    const char
      *c="This string will not be destroyed with free() or munmap().\n",
      *h="This string is on the heap and will be removed with free().\n",
      *m="This string is stored as an (annonymous) memory mapped file and will be destroyed with munmap().\n";
    C(handle,ANSI_INVERSE"DEMONSTRATING DYNAMICALLY GENERATED FILE CONTENT"ANSI_RESET"\n\nUsers can implement file generation by customizing the source file "ANSI_FG_BLUE,0);
    C(handle,src+1+cg_last_slash(src),0);
    C(handle,ANSI_RESET" and recompilation.\n\n",0);
    C(handle,"The following demonstrates three storage methods.\nSince they are null terminated strings, we can give zero for the data length.\n\n",0);
    C(handle,c,strlen(c));
    C(handle,c,0);
    C(handle,"\n",0);
    H(handle,strdup(h),strlen(h));
    H(handle,strdup(h),0);
    {
      C(handle,"\n\n"ANSI_INVERSE"Demonstrating exec"ANSI_RESET"\n",0);
      char *cmd[]={"date",NULL};
      X(handle,0,cmd,NULL);
    }
    C(handle,"\n",1);
    {
      C(handle,"\n\n"ANSI_INVERSE"Demonstrating exec with environment variables"ANSI_RESET"\n",0);
      char *cmd[]={"sh","-c","echo my_environment_variable='$my_environment_variable'",NULL};
      char *env[]={"my_environment_variable=Hello world",NULL};
      X(handle,ZIPSFS_C_MMAP,cmd,env);
    }
    C(handle,"\n",1);
    const int m_l=strlen(m);

#define _config_c_to_mmap(bytes,size) memcpy(cg_mmap(0,size,0),bytes,size)
    M(handle,_config_c_to_mmap(m,m_l),m_l);
    M(handle,_config_c_to_mmap(m,m_l),0);
    C(handle,"\n",1);
    FSIZE_TO_HASHTABLE(handle,vp,vp_l);
    return true;
  }
  return false;
}
