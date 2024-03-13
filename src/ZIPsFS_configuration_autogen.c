struct _struct_generated{
  const char *ext,*tail,*match;
  int ext_l,tail_l;
  const char *cmd[];
}
  _generated_test1={.ext=".test1.ext",.tail=".raw",.cmd={""}},
  *_generated_array[]={&_generated_test1,NULL};
static struct _struct_generated *_generated_i(const int i){
  struct _struct_generated *s=_generated_array[i];
  if (!s->ext_l) s->ext_l=strlen(s->ext);
  if (!s->tail_l) s->tail_l=strlen(s->tail);
  return s;
}

#define FOREACH_GENERATED(s)  struct _struct_generated *s; for(int i=0;(s=_generated_i(i));i++)
static bool config_generated_from_virtualpath(char *generated,const char *vp,const int vp_l){
  FOREACH_GENERATED(s){
    if (vp_l>s->tail_l && !memcmp(vp+vp_l-s->tail_l,s->tail,s->tail_l) && (!s->match || strstr(vp,s->match))){
      memcpy(generated,vp,vp_l);
      memcpy(generated+vp_l,s->ext,s->ext_l);
      generated[vp_l+s->ext_l]=0;
    }
  }
  return false;
}

static bool config_generated_to_virtualpath(const bool create,const char *generated,const int generated_l,char *vp){
  FOREACH_GENERATED(s){
    const int vp_l=generated_l-s->ext_l-s->tail_l;
    if (vp_l>0 &&
        !memcmp(generated+generated_l-s->ext_l,s->ext,s->ext_l) &&
        !memcmp(generated+generated_l-s->ext_l-s->tail_l,s->tail,s->tail_l) &&
        (!s->match || strstr(generated,s->match))){
      memcpy(vp,generated,vp_l);
      vp[vp_l]=0;
    }
  }
  return false;
}
