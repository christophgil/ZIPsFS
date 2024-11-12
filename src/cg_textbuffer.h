#ifndef cg_textbuffer_dot_h
#define cg_textbuffer_dot_h

#define TEXTBUFFER_DIM_STACK 64
#define TEXTBUFFER_MMAP (1<<0)
#define TEXTBUFFER_NEVER_DESTROY (1<<1)
struct textbuffer{
  int flags; /* Otherwise heap */
  int n; /* Number segments contained */
  int capacity; /* Number segments allocated */
  off_t *segment_e, _onstack_segment_e[TEXTBUFFER_DIM_STACK]; /* Next  position after this segment */
  char **segment,*_onstack_segment[TEXTBUFFER_DIM_STACK]; /* Segments */
  off_t max_length;
  int read_bufsize;
};
#endif
