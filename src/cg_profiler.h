////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                          ///
/// Counting CPU cycles of function calls        ///
/// See cg_profiler_make_files.sh                ///
////////////////////////////////////////////////////

#ifndef WITH_PROFILER
#define WITH_PROFILER 0
#elif  WITH_PROFILER
#undef PROFILED
#define PROFILED_REAL(x) x##_PROFILED
#define PROFILED(x) PROFILED_REAL(x)
#define PROFILER_B(f)   const clock_t c=clock();_count_fn[PROFILE_##f]++
#define PROFILER_E(f)   _clock_fn[PROFILE_##f]+=(long)(clock()-c)
// ---
#define C(x) PROFILE_##x,
enum ENUM_PROFILE_FN{
#include "generated_profiler_names.c"
                     PROFILE_LENGTH};
#undef C
// ---
static long _clock_fn[PROFILE_LENGTH], _count_fn[PROFILE_LENGTH];
void print_profile(){
  fprintf(stderr,"\n"ANSI_INVERSE"Profiling functions"ANSI_RESET"\nCLOCKS_PER_SEC: %'ld\n\n",CLOCKS_PER_SEC);
  fprintf(stderr,ANSI_UNDERLINE"%42s   # Calls   Time-Sum[s] "ANSI_RESET"\n","Function");
#define C(f) fprintf(stderr, "%42s %'9ld  %'12.1f\n\n",#f,_count_fn[PROFILE_##f],((float)_clock_fn[PROFILE_##f])/CLOCKS_PER_SEC);
#include "generated_profiler_names.c"
#undef C
  fputc('\n',stderr);
}
// ---
#include "generated_profiler.h" /* By cg_profiler_make_files.sh */
#endif //WITH_PROFILER
