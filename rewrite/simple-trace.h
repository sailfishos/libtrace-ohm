#ifndef __SIMPLE_TRACE_H__
#define __SIMPLE_TRACE_H__

#include <sys/time.h>

#if !defined(likely) || !defined(unlikely)
#  undef likely
#  undef unlikely
#  ifdef __GNUC__
#    define likely(cond)   __builtin_expect((cond), 1)
#    define unlikely(cond) __builtin_expect((cond), 0)
#  else
#    define likely(cond)   (cond)
#    define unlikely(cond) (cond)
#  endif
#endif


#if !defined(FALSE) || !defined(TRUE)
#  undef FALSE
#  undef TRUE
#  define FALSE 0
#  define TRUE  1
#endif


/*
 * a trace flag
 */

#define TRACE_FLAG(n, d, fptr) {                          \
        .name    = n,                                     \
        .descr   = d,                                     \
        .bit     = -1,                                    \
        .flagptr = fptr,                                  \
    }

#define TRACE_FLAG_END { .name = NULL, .descr = NULL }


typedef struct {
    char *name;                              /* symbolic flag name */
    char *descr;                             /* description of flag */
    int   bit;                               /* allocated bit in module */
    int  *flagptr;                           /* reported to trace 'client' */
} trace_flag_t;



/*
 * a trace module (ie. a set of trace flags)
 */

#define TRACE_DECLARE_MODULE(v, n, ...)                                   \
    trace_flag_t __trace_flags_##v[] = {                                  \
        __VA_ARGS__,                                                      \
        TRACE_FLAG_END,                                                   \
    };                                                                    \
    trace_module_t v = {                                                  \
        .name  = (n),                                                     \
        .flags = __trace_flags_##v,                                       \
        .nflag = sizeof(__trace_flags_##v) / sizeof(__trace_flags_##v[0]) \
    }


typedef struct {
    char         *name;                      /* symbolic module name */
    trace_flag_t *flags;                     /* trace flags of this module */
    int           nflag;                     /* number of flags */
    int           id;                        /* module id within context */
} trace_module_t;



/*
 * a trace context (a set of trace modules)
 */

typedef struct {
    union {
        unsigned long  word;                 /* 32 or less dense bits */
        unsigned long *wptr;                 /* > 32 or sparse bits */
    } bits;
    int                nbit;
} trace_bits_t;


typedef struct {
    char           *name;                    /* symbolic context name */
    char           *format;                  /* trace format */
    FILE           *destination;             /* destination for messages */
    int             disabled;                /* global state of this context */
    trace_bits_t    bits;                    /* allocated bits */
    trace_bits_t    mask;                    /* current state of flags */
    trace_module_t *modules;                 /* actual modules */
    int             nmodule;                 /* number of modules */
    int             id;                      /* context id */
    struct timeval  prev;                    /* timestamp of last message */
} trace_context_t;


/*
 * trace destinations
 */

#define TRACE_TO_STDERR     ((char *)0x0)
#define TRACE_TO_STDOUT     ((char *)0x1)
#define TRACE_TO_FILE(path) (path)

#define trace_write(id, format, args...)        \
    __trace_write(id, __FILE__, __LINE__, __FUNCTION__, format"\n", ## args)



/*
 * public API
 */

int  trace_init(void);
void trace_exit(void);

int  trace_context_add(const char *name);
int  trace_context_del(int cid);
int  trace_context_format(int cid, const char *format);
int  trace_context_target(int cid, const char *target);
int  trace_context_enable(int cid);
int  trace_context_disable(int cid);

int  trace_module_add(int cid, trace_module_t *module);
int  trace_module_del(int cid, const char *name);

int  trace_flag_set(int id);
int  trace_flag_clr(int id);
int  trace_flag_tst(int id);

int  trace_set_flags(const char *config);

void __trace_write(int id, const char *file, int line, const char *func,
                   const char *format, ...);






#if 0

int  trace_open (char *name);
void trace_close(int context);

int  trace_set_target(trace_context_t *ctx, const char *target);
int  trace_set_format(trace_context_t *ctx, const char *format);

int  trace_enable(trace_context_t *ctx);
int  trace_disable(trace_context_t *ctx);

int  trace_set(trace_context_t *ctx, int flag);
int  trace_clr(trace_context_t *ctx, int flag);

int  trace_message(trace_context_t *ctx, int flag, const char *format, ...);

int  trace_configure(const char *request);

#endif



/*
 * memory allocation wrapper macros
 */

#ifndef ALLOC

#define ALLOC(type) ({                            \
            type   *__ptr;                        \
            size_t  __size = sizeof(type);        \
                                                  \
            if ((__ptr = malloc(__size)) != NULL) \
                memset(__ptr, 0, __size);         \
            __ptr; })

#define ALLOC_OBJ(ptr) ((ptr) = ALLOC(typeof(*ptr)))

#define ALLOC_ARR(type, n) ({                     \
            type   *__ptr;                        \
            size_t   __size = (n) * sizeof(type); \
                                                  \
            if ((__ptr = malloc(__size)) != NULL) \
                memset(__ptr, 0, __size);         \
            __ptr; })

#define REALLOC_ARR(ptr, o, n) ({                                       \
            typeof(ptr) __ptr;                                          \
            size_t      __size = sizeof(*ptr) * (n);                    \
                                                                        \
            if ((ptr) == NULL) {                                        \
                (__ptr) = ALLOC_ARR(typeof(*ptr), n);                   \
                ptr = __ptr;                                            \
            }                                                           \
            else if ((__ptr = realloc(ptr, __size)) != NULL) {          \
	      if ((unsigned)(n) > (unsigned)(o))			\
                    memset(__ptr + (o), 0, ((n)-(o)) * sizeof(*ptr));   \
                ptr = __ptr;                                            \
            }                                                           \
            __ptr; })

#define ALLOC_VAROBJ(ptr, n, f) ({					\
      size_t __diff = ((ptrdiff_t)(&(ptr)->f[n])) - ((ptrdiff_t)(ptr)); \
      ptr = (typeof(ptr))ALLOC_ARR(char, __diff);			\
      ptr; })
                
#define FREE(obj) do { if (obj) free(obj); } while (0)

#define STRDUP(s) ({						\
      char *__s;						\
      if (__builtin_types_compatible_p(typeof(s), char []))	\
	__s = strdup(s);					\
      else							\
	__s = ((s) ? strdup(s) : strdup(""));			\
      __s; })


#endif /* !ALLOC */




#endif /* __SIMPLE_TRACE_H__ */


/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */



