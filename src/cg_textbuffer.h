#ifndef cg_textbuffer_dot_h
#define cg_textbuffer_dot_h

#define TEXTBUFFER_DIM_STACK 64

struct textbuffer{
  int malloc_id;
#define TEXTBUFFER_ENOMEM (1<<1)
#define TEXTBUFFER_DESTROYED (1<<2)
  int flags;
  int n; /* Number segments contained */
  int capacity; /* Number segments allocated */
  off_t *_segment_e, _onstack_segment_e[TEXTBUFFER_DIM_STACK]; /* Next  position after this segment */
  char **_segment, *_onstack_segment[TEXTBUFFER_DIM_STACK]; /* Segments */
  uint8_t *_segment_flags, _onstack_segment_flags[TEXTBUFFER_DIM_STACK]; /* See TXTBUFSGMT_MUNMAP and TXTBUFSGMT_NO_FREE */


#define TXTBUFSGMT_NO_FREE  (1<<2) /* The destructor will not free the text segment. See ->segment_flags */
#define TXTBUFSGMT_MUNMAP   (1<<3) /* The destructor will  free the text segment with munmap() rather than free(). */
#define TXTBUFSGMT_DUP      (1<<4) /* A copy of the binary data  rather than the data itself will be created on the heap and stored as a segment */
#define TXTBUFSGMT_NO_COUNT (1<<5)  /* Do not decrement counter on munmap/free */


  #define TEXTBUFFER_SET_ENOMEM(b) b->flags|=TEXTBUFFER_ENOMEM


  off_t max_length;
  int read_bufsize;
};
typedef struct textbuffer textbuffer_t;
#define  textbuffer_first_segment(b)  b->_onstack_segment[0]

enum enum_exec_on_file{EXECF_MOUNTPOINT_USING_DF,EXECF_MOUNTPOINT_USING_FINDMNT, EXECF_NUM};
#define EXECF_SILENT (1<<1)

//atomic_int _
#endif
