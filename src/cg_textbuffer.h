#ifndef cg_textbuffer_dot_h
#define cg_textbuffer_dot_h

#define TEXTBUFFER_DIM_STACK 64


#define TEXTBUFFER_NODESTROY (1<<1) /* The destructor will not free the text segment. See ->segment_flags */
#define TEXTBUFFER_MUNMAP (1<<2)    /* The destructor will  free the text segment with munmap() rather than free(). */


#define TEXTBUFFER_ENOMEM (1<<1)
#define TEXTBUFFER_SET_ENOMEM(b) b->flags|=TEXTBUFFER_ENOMEM
struct textbuffer{
  int flags;
  int n; /* Number segments contained */
  int capacity; /* Number segments allocated */
  off_t *segment_e, _onstack_segment_e[TEXTBUFFER_DIM_STACK]; /* Next  position after this segment */
  char **segment, *_onstack_segment[TEXTBUFFER_DIM_STACK]; /* Segments */
  u_int8_t *segment_flags,_onstack_segment_flags[TEXTBUFFER_DIM_STACK]; /* See TEXTBUFFER_MUNMAP and TEXTBUFFER_NODESTROY */
  off_t max_length;
  int read_bufsize;
};
#endif
