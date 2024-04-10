#ifndef GCC_SYSTEM_H
#define GCC_SYSTEM_H
#ifndef WIFSIGNALED
#define WIFSIGNALED(S) (((S) & 0xff) != 0 && ((S) & 0xff) != 0x7f)
#endif
#ifndef WTERMSIG
#define WTERMSIG(S) ((S) & 0x7f)
#endif
#ifndef WIFEXITED
#define WIFEXITED(S) (((S) & 0xff) == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(S) (((S) & 0xff00) >> 8)
#endif
#ifndef WSTOPSIG
#define WSTOPSIG WEXITSTATUS
#endif
#ifndef WCOREDUMP
#define WCOREDUMP(S) ((S) & WCOREFLG)
#endif
#ifndef WCOREFLG
#define WCOREFLG 0200
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* If the system doesn't provide strsignal, we get it defined in
   libiberty but no declaration is supplied.  */
#if !defined (HAVE_STRSIGNAL) \
    || (defined (HAVE_DECL_STRSIGNAL) && !HAVE_DECL_STRSIGNAL)
# ifndef strsignal
extern const char *strsignal (int);
# endif
#endif

#ifdef __cplusplus
}
#endif

#endif
