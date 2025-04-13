/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// For efficient cache simplify_name                         ///
/////////////////////////////////////////////////////////////////
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
static bool zipentry_placeholder_expand(char *n,const char *zipfile){
  const char *placeholder=strchr(n,PLACEHOLDER_NAME);
  if (placeholder){
    const char *replacement=zipfile+cg_last_slash(zipfile)+1;
    const int n_l=cg_strlen(n),pos=placeholder-n,replacement_l=((char*)cg_strchrnul(replacement,'.'))-replacement;
    int dst=n_l+replacement_l;/* Start copying with terminal zero */
    if (dst>=MAX_PATHLEN) return false;
    for(;--dst>=0;){
      const int src=dst<pos?dst:  dst<pos+replacement_l?-1: dst-replacement_l+1;
      if (src>=0) n[dst]=n[src];
    }
    memcpy(n+pos,replacement,replacement_l);
  }
  return true;
}
