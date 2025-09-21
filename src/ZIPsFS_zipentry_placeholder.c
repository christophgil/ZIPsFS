/////////////////////////////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                                       ///
/// Consider ZIP file 20220802_Z1_AF_001_30-0001_SWATH_P01-Rep4.wiff2.Zip                     ///
/// 20220802_Z1_AF_001_30-0001_SWATH_P01-Rep4.wiff.scan  will be stored as *.wiff.scan.       ///
/// * denotes the PLACEHOLDER_NAME                                                            ///
/// This will save space in cache.                                                            ///
/// Many ZIP files will have identical table-of-content and thus kept only once in the cache. ///
/////////////////////////////////////////////////////////////////////////////////////////////////


static const char *zipentry_placeholder_insert(char *s,const char *u, const char *zipfile){
  const int ulen=cg_strlen(u);
  if (strchr(u,PLACEHOLDER_NAME)){
    static int alreadyWarned=0;
    if(!alreadyWarned++) warning(WARN_CHARS,zipfile,"Found PLACEHOLDER_NAME in file name");
  }
  s[0]=0;
  if (!ulen) return u;
  if (zipfile && (ulen<MAX_PATHLEN) && *zipfile){
    const char *replacement=zipfile+cg_last_slash(zipfile)+1, *dot=cg_strchrnul(replacement,'.');
    const int replacement_l=dot-replacement;
    const char *posaddr=dot && replacement_l>5?cg_memmem(u,ulen,replacement,replacement_l):NULL;
    if (posaddr && posaddr<replacement+replacement_l){
      const int pos=posaddr-u;
      memcpy(s,u,pos);
      s[pos]=PLACEHOLDER_NAME;
      memcpy(s+pos+1,u+pos+replacement_l,ulen-replacement_l-pos);
      s[ulen-replacement_l+1]=0;
      return s;
    }
  }
  return u;
}

/*  Reverses  */
static int zipentry_placeholder_expand2(char *n,const char *zipfile){
  const char *placeholder=strchr(n,PLACEHOLDER_NAME);
  const int n_l=cg_strlen(n);
  int len=n_l;
  if (placeholder){
    const char *replacement=zipfile+cg_last_slash(zipfile)+1;
    const int pos=placeholder-n,replacement_l=((char*)cg_strchrnul(replacement,'.'))-replacement;
    len=n_l+replacement_l-1;
    if (len>MAX_PATHLEN){
      warning(WARN_STR|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_ERROR,n,"Exceed_MAX_PATHLEN");
      return n_l;
    }
    for(int dst=len+1;--dst>=0;){ /* Start copying with terminal zero */
      const int src=dst<pos?dst:  dst<pos+replacement_l?-1: dst-replacement_l+1;
      if (src>=0) n[dst]=n[src];
    }
    memcpy(n+pos,replacement,replacement_l);
  }
  ASSERT(len==strlen(n));
  return len;
}
