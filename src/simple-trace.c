#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <locale.h>
#include <sys/time.h>

#include <simple-trace/simple-trace.h>
#include "mm.h"


#define STAMP_FORMAT    "%Y-%b-%d %H:%M:%S"
#define STAMP_STRFLEN   (4+1+3+1+2+1+2+1+2+1+2)  /* YYYY-MMM-DD hh:mm:ss */
#define STAMP_MSECLEN   (1+3)                    /*                     .mmm */
#define STAMP_SIZE      (STAMP_STRFLEN + STAMP_MSECLEN + 1)
#define STAMP_UNKNOWN   "???? ??? ?? ??:??:??"


#define BITS_PER_LONG  ((int)sizeof(unsigned long) * 8)
#define BYTES_PER_LONG ((int)sizeof(unsigned long))

#define MAX_CONTEXTS  127
#define CTX_SHIFT      24
#define CTX_MASK     0xff
#define MAX_MODULES   256
#define MOD_SHIFT      16
#define MOD_MASK     0xff
#define MAX_FLAGS     256
#define IDX_SHIFT       8
#define IDX_MASK     0xff
#define BIT_MASK     0xff

#define FLAG_ID(c, m, i, b)                 \
    ((((c) & CTX_MASK) << CTX_SHIFT) |      \
     (((m) & MOD_MASK) << MOD_SHIFT) |      \
     (((i) & IDX_MASK) << IDX_SHIFT) |      \
      ((b) & BIT_MASK))

#define FLAG_CTX(id) (((id) >> CTX_SHIFT))
#define FLAG_MOD(id) (((id) >> MOD_SHIFT) & MOD_MASK)
#define FLAG_IDX(id) (((id) >> IDX_SHIFT) & IDX_MASK)
#define FLAG_BIT(id) ( (id)               & BIT_MASK)


#define MAX_NAME 128




/*
 * a trace flag
 */

typedef struct {
    char *name;                              /* symbolic flag name */
    char *descr;                             /* description of flag */
    int   bit;                               /* allocated bit in module */
    int  *flagptr;                           /* 'client' pointer to update */
} flag_t;


/*
 * a trace module is a named set of trace flags
 */

typedef struct {
    char   *name;                            /* symbolic module name */
    flag_t *flags;                           /* trace flags of this module */
    int     nflag;                           /* number of flags */
    int     id;                              /* module id within context */
} module_t;


/*
 * a trace context is a named set of trace modules
 */

typedef struct {
    union {
        unsigned long  word;                 /* 32 or less dense bits */
        unsigned long *wptr;                 /* > 32 or sparse bits */
    } bits;
    int                nbit;
} bitmap_t;


typedef struct {
    char           *name;                    /* symbolic context name */
    char           *format;                  /* trace format */
    FILE           *destination;             /* destination for messages */
    int             disabled;                /* global state of this context */
    bitmap_t        bits;                    /* allocated bits */
    bitmap_t        mask;                    /* current state of flags */
    module_t       *modules;                 /* actual modules */
    int             nmodule;                 /* number of modules */
    int             id;                      /* context id */
    struct timeval  prev;                    /* timestamp of last message */
} context_t;


#define CONTEXT_LOOKUP(cid) ({                                          \
            context_t *_ctx;                                            \
            if (unlikely(cid < 0 || cid >= ncontext))                   \
                _ctx = NULL;                                            \
            else                                                        \
                _ctx = contexts[cid].name ? contexts + (cid) : NULL;    \
            _ctx;})

#define MODULE_LOOKUP(ctx, id) ({                       \
            module_t *_m;                               \
            if ((ctx) != NULL) {                        \
                if (0 <= (id) && (id) < (ctx)->nmodule) \
                    _m = (ctx)->modules[id].name ?      \
                        (ctx)->modules + (id) : NULL;   \
                else                                    \
                    _m = NULL;                          \
            }                                           \
            else                                        \
                _m = NULL;                              \
            _m;})

#define FLAG_LOOKUP(mod, idx) ({                        \
            flag_t *_f;                                 \
            if ((mod) != NULL) {                        \
                if (0 <= (idx) && (idx) < (mod)->nflag) \
                    _f = (mod)->flags + (idx);          \
                else                                    \
                    _f = NULL;                          \
            }                                           \
            else                                        \
                _f = NULL;                              \
            _f;})


#define INFO(fmt, args...) do {                                         \
        fprintf(stdout, "[INFO] "fmt"\n", ## args);                     \
        fflush(stdout);                                                 \
    } while (0)

#define WARNING(fmt, args...) do {                                       \
        fprintf(stderr, "[WARNING] %s: "fmt"\n", __FUNCTION__, ## args); \
        fflush(stderr);                                                  \
    } while (0)

#define ERROR(fmt, args...) do {                                        \
        fprintf(stderr, "[ERROR] %s: "fmt"\n", __FUNCTION__, ## args);  \
        fflush(stderr);                                                 \
    } while (0)



static context_t *contexts;
static int        ncontext;
static int        initialized    = FALSE;
static char      *default_format = TRACE_DEFAULT_FORMAT;

static int        context_init(context_t *ctx, const char *name);
static context_t *context_find(const char *name, context_t **deleted);
static void       context_del (context_t *ctx);

static module_t *module_find(context_t *ctx, const char *name,
                             module_t **deleted);
static void      module_free(context_t *ctx, module_t *module);

static flag_t *flag_find(module_t *module, const char *name, flag_t **deleted);


static inline int alloc_flag(context_t *ctx);
static void       init_bits (bitmap_t *bits);
static void       free_bits (bitmap_t *bits);

static inline int clr_bit(bitmap_t *tb, int n);
static inline int set_bit(bitmap_t *tb, int n);
static inline int tst_bit(bitmap_t *tb, int n);


static int check_format(const char *format);
static int format_message(context_t *ctx, int id,
                          const char *file, int line, const char *func,
                          char *buf, int bufsize,
                          const char *fmt, va_list args);


/********************
 * trace_init
 ********************/
int
trace_init(void)
{
    int err;
    
    if (initialized)
        return 0;

    if ((contexts = ALLOC_ARR(context_t, 1)) == NULL)
        return -ENOMEM;

    if ((err = context_init(contexts, TRACE_DEFAULT_NAME)) < 0)
        return err;
    
    ncontext    = 1;
    initialized = TRUE;
    
    return 0;
}


/********************
 * trace_exit
 ********************/
void
trace_exit(void)
{
    context_t *c;
    int              i;
    
    for (i = 0; i < ncontext; i++) {
        c = contexts + i;
        context_del(c);
    }
    
    FREE(contexts);
    contexts = NULL;
    ncontext = 0;
}


/********************
 * trace_context_open
 ********************/
int
trace_context_open(const char *name)
{
    context_t *ctx, *deleted;

    if (!initialized)
        trace_init();

    if ((ctx = context_find(name, &deleted)) != NULL)
        return ctx->id;

    if (deleted == NULL) {
        if (ncontext >= MAX_CONTEXTS)
            return -ENOSPC;
        if (REALLOC_ARR(contexts, ncontext, ncontext + 1) == NULL)
            return -ENOMEM;
        ctx = contexts + ncontext;
    }
    else
        ctx = deleted;
    
    return context_init(ctx, name);
}


/********************
 * trace_context_close
 ********************/
int
trace_context_close(int cid)
{
    context_t *ctx = CONTEXT_LOOKUP(cid);
    
    if (ctx == contexts)
        return 0;

    if (ctx == NULL)
        return -ENOENT;
    
    context_del(ctx);
    if (cid == ncontext - 1) {
        if (ncontext > 1)
            REALLOC_ARR(contexts, ncontext, ncontext - 1);
        else {
            FREE(contexts);
            contexts = NULL;
        }
        ncontext--;
    }
    
    return 0;
}


/********************
 * trace_context_enable
 ********************/
int
trace_context_enable(int cid)
{
    context_t *ctx = CONTEXT_LOOKUP(cid);

    if (ctx == NULL)
        return -ENOENT;
    
    ctx->disabled = FALSE;
    return 0;
}


/********************
 * trace_context_disable
 ********************/
int
trace_context_disable(int cid)
{
    context_t *ctx = CONTEXT_LOOKUP(cid);

    if (ctx == NULL)
        return -ENOENT;

    ctx->disabled = TRUE;
    return 0;
}


/********************
 * context_target
 ********************/
static int
context_target(context_t *ctx, const char *target)
{
    FILE *nfp, *ofp;

    ofp = ctx->destination;

    if      (target == TRACE_TO_STDERR) nfp = stderr;
    else if (target == TRACE_TO_STDOUT) nfp = stdout;
    else if (!strcmp(target, "stderr")) nfp = stderr;
    else if (!strcmp(target, "stdout")) nfp = stdout;
    else                                nfp = fopen(target, "a");
    
    if (nfp == NULL)
        return -errno;
    
    if (ofp != NULL && ofp != stderr && ofp != stdout)
        fclose(ofp);
    
    ctx->destination = nfp;
    return 0;
}


/********************
 * trace_context_target
 ********************/
int
trace_context_target(int cid, const char *target)
{
    context_t *ctx = CONTEXT_LOOKUP(cid);

    if (ctx != NULL)
        return context_target(ctx, target);
    else
        return -ENOENT;
}


/********************
 * context_format
 ********************/
int
context_format(context_t *ctx, const char *format)
{
    int err;
    
    if ((err = check_format(format)) < 0)
        return err;
    else {
        if (ctx->format != default_format)
            FREE(ctx->format);
        if ((ctx->format = STRDUP(format)) == NULL) {
            ctx->format = default_format;
            return -ENOMEM;
        }
    }
    
    return 0;
}


/********************
 * trace_context_format
 ********************/
int
trace_context_format(int cid, const char *format)
{
    context_t *ctx = CONTEXT_LOOKUP(cid);
    
    if (ctx != NULL)
        return context_format(ctx, format);
    else
        return -ENOENT;
}


/********************
 * trace_flag_set
 ********************/
int
trace_flag_set(int id)
{
    context_t *ctx;
    module_t  *mod;
    flag_t    *flg;
    int        c, m, i, b;
    
    c = FLAG_CTX(id);
    m = FLAG_MOD(id);
    i = FLAG_IDX(id);
    b = FLAG_BIT(id);
    
    ctx = CONTEXT_LOOKUP(c);
    mod = MODULE_LOOKUP(ctx, m);
    flg = FLAG_LOOKUP(mod, i);

    if (unlikely(flg == NULL))
        return -ENOENT;

    if (unlikely(flg->bit != b))
        return -EINVAL;
    
    return set_bit(&ctx->mask, flg->bit);
}


/********************
 * trace_flag_clr
 ********************/
int
trace_flag_clr(int id)
{
    context_t *ctx;
    module_t  *mod;
    flag_t    *flg;
    int        c, m, i, b;
    
    c = FLAG_CTX(id);
    m = FLAG_MOD(id);
    i = FLAG_IDX(id);
    b = FLAG_BIT(id);

    ctx = CONTEXT_LOOKUP(c);
    mod = MODULE_LOOKUP(ctx, m);
    flg = FLAG_LOOKUP(mod, i);

    if (unlikely(flg == NULL))
        return -ENOENT;
    
    if (unlikely(flg->bit != b))
        return -EINVAL;
    
    return clr_bit(&ctx->mask, flg->bit);
}


/********************
 * trace_flag_tst
 ********************/
int
trace_flag_tst(int id)
{
    context_t *ctx;
    module_t  *mod;
    flag_t    *flg;
    int        c, m, i, b;
    
    c = FLAG_CTX(id);
    m = FLAG_MOD(id);
    i = FLAG_IDX(id);
    b = FLAG_BIT(id);

    ctx = CONTEXT_LOOKUP(c);
    mod = MODULE_LOOKUP(ctx, m);
    flg = FLAG_LOOKUP(mod, i);

    if (unlikely(flg == NULL))
        return -ENOENT;

    if (unlikely(flg->bit != b))
        return - EINVAL;
    
    return tst_bit(&ctx->mask, flg->bit);
}


/********************
 * __trace_printf
 ********************/
int
__trace_printf(int id, const char *file, int line, const char *func,
               const char *format, ...)
{
    int        cid = FLAG_CTX(id);
    context_t *ctx = CONTEXT_LOOKUP(cid);
    va_list    ap;
    char       buf[4096];
    int        n;
    
    if (ctx == NULL)
        return -ENOENT;
    
    if (ctx->disabled || !trace_flag_tst(id))
        return 0;
    
    va_start(ap, format);
    n = format_message(ctx, id, file, line, func, buf, sizeof(buf), format, ap);
    va_end(ap);
    if (n < 0)
        return n;
    fflush(ctx->destination);
    n = write(fileno(ctx->destination), buf, n - 1);

    return n;
}


/********************
 * context_init
 ********************/
static int
context_init(context_t *ctx, const char *name)
{
    if ((ctx->name = STRDUP(name)) == NULL)
        return -ENOMEM;
    
    ctx->format      = default_format;
    ctx->destination = stderr;

    init_bits(&ctx->bits);
    init_bits(&ctx->mask);

    ctx->id = ((int)((void *)ctx - (void *)contexts)) / sizeof(*ctx);

    return ctx->id;
}


/********************
 * context_del
 ********************/
static void
context_del(context_t *ctx)
{
    int i;

    FREE(ctx->name);
    ctx->name = NULL;

    if (ctx->format != default_format)
        FREE(ctx->format);
    ctx->format = NULL;
    
    if (ctx->destination != stderr && ctx->destination != stdout) {
        fflush(ctx->destination);
        fclose(ctx->destination);
        ctx->destination = NULL;
    }
    
    for (i = 0; i < ctx->nmodule; i++)
        module_free(ctx, ctx->modules + i);
    
    FREE(ctx->modules);
    ctx->modules = NULL;
    ctx->nmodule = 0;
    
    free_bits(&ctx->bits);
    free_bits(&ctx->mask);
}


/********************
 * context_find
 ********************/
static context_t *
context_find(const char *name, context_t **deleted)
{
    context_t *ctx;
    int              i;

    if (deleted != NULL)
        *deleted = NULL;
    
    for (i = 0; i < ncontext; i++) {
        ctx = contexts + i;
        if (ctx->name == NULL) {
            if (deleted != NULL && *deleted == NULL)
                *deleted = ctx;
            continue;
        }
        if (!strcmp(ctx->name, name))
            return ctx;
    }

    return NULL;
}


/********************
 * trace_add_module
 ********************/
int
trace_add_module(int cid, trace_moduledef_t *moddef)
{
    context_t       *ctx = CONTEXT_LOOKUP(cid);
    trace_flagdef_t *flagdef;
    module_t        *mod, *deleted;
    flag_t          *flag;
    int              i, nflag, n;

    
    if (ctx == NULL)
        return -ENOENT;

    if (moddef->name == NULL) {
        WARNING("Module with NULL name for context %s.", ctx->name);
        return -EINVAL;
    }
    
    if (module_find(ctx, moddef->name, &deleted) != NULL) {
        WARNING("Context %s already has a module %s.", ctx->name,
                      moddef->name);
        return -EEXIST;
    }

    nflag = 0;
    for (i = 0, flagdef = moddef->flags; i < moddef->nflag; i++, flagdef++) {
        if (flagdef->name == NULL || flagdef->flagptr == NULL) {
            if (i == moddef->nflag - 1)  /* it's okay to NULL-terminate */
                continue;
            WARNING("#%d flag of %s.%s is invalid.", i + 1, ctx->name,
                          moddef->name);
            return -EINVAL;
        }
        nflag++;
    }

    if (deleted == NULL) {
        n = ctx->nmodule;
        if (REALLOC_ARR(ctx->modules, n, n + 1) == NULL)
            return -ENOMEM;
        
        mod = ctx->modules + ctx->nmodule;
        mod->id = ctx->nmodule++;
    }
    else
        mod = deleted;
    
    if ((mod->name = STRDUP(moddef->name)) == NULL)
        return - ENOMEM;
    
    if ((mod->flags = ALLOC_ARR(typeof(*mod->flags), nflag)) == NULL)
        return -ENOMEM;
    
    mod->nflag = nflag;

    /* save module and allocate flag bits */
    for (i = 0; i < nflag; i++) {
        flag    = mod->flags + i;
        flagdef = moddef->flags + i;
        if ((flag->name  = STRDUP(flagdef->name))  == NULL ||
            (flag->descr = STRDUP(flagdef->descr)) == NULL ||
            (flag->bit   = alloc_flag(ctx)) < 0) {
            module_free(ctx, mod);
            return -ENOMEM;
        }
        
        if (flag->bit >= MAX_FLAGS) {
            module_free(ctx, mod);
            return -EOVERFLOW;
        }
        
        *flagdef->flagptr = FLAG_ID(ctx->id, mod->id, i, flag->bit);
        flag->flagptr     = flagdef->flagptr;
    }
    
    return 0;
}


/********************
 * trace_del_module
 ********************/
int
trace_del_module(int cid, const char *name)
{
    context_t *ctx;
    module_t  *module;

    if (name == NULL)
        return -EINVAL;
    
    if ((ctx = CONTEXT_LOOKUP(cid)) == NULL)
        return -ENOENT;
    
    if ((module = module_find(ctx, name, NULL)) == NULL)
        return -ENOENT;
    
    module_free(ctx, module);
    if (module->id == ctx->nmodule - 1) {
        if (ctx->nmodule > 1)
            REALLOC_ARR(ctx->modules, ctx->nmodule, ctx->nmodule - 1);
        else {
            FREE(ctx->modules);
            ctx->modules = NULL;
        }
        ctx->nmodule--;
    }
    
    return 0;
}


/********************
 * module_find
 ********************/
static module_t *
module_find(context_t *ctx, const char *name, module_t **deleted)
{
    module_t *m;
    int       i;

    if (deleted != NULL)
        *deleted = NULL;
    
    for (i = 0, m = ctx->modules; i < ctx->nmodule; i++, m++) {
        if (m->name == NULL) {
            if (deleted != NULL && *deleted == NULL)
                *deleted = m;
            continue;
        }
        if (!strcmp(m->name, name))
            return m;
    }
    
    return NULL;
}
 
 
/********************
 * module_free
 ********************/
static void
module_free(context_t *ctx, module_t *module)
{
    flag_t *flag;
    int     i;

    FREE(module->name);
    module->name = NULL;

    for (i = 0, flag = module->flags; i < module->nflag; i++, flag++) {
        FREE(flag->name);
        FREE(flag->descr);
        flag->name  = NULL;
        flag->descr = NULL;
        if (ctx != NULL) {
            clr_bit(&ctx->bits, flag->bit);
            clr_bit(&ctx->mask, flag->bit);
        }
    }
    FREE(module->flags);
    module->flags = NULL;
    module->nflag = 0;
}



/********************
 * flag_find
 ********************/
static flag_t *
flag_find(module_t *module, const char *name, flag_t **deleted)
{
    flag_t *f;
    int     i;

    if (deleted != NULL)
        *deleted = NULL;
    
    for (i = 0, f = module->flags; i < module->nflag; i++, f++) {
        if (f->name == NULL) {
            if (deleted != NULL && *deleted == NULL)
                *deleted = f;
            continue;
        }
        if (!strcmp(f->name, name))
            return f;
    }
    
    return NULL;
}


/*****************************************************************************
 *                           *** message formatting ***                      *
 *****************************************************************************/

/********************
 * get_timestamp
 ********************/
static char *
get_timestamp(char *buf, struct timeval *tv)
{
    struct tm  tm;
    time_t     now;
    int        ms;
    char      *d, *lc_time;

    /* must have buffer of TIMESTAMP_SIZE or more bytes */
    
    if (unlikely(gettimeofday(tv, NULL) < 0)) {
        strcpy(buf, STAMP_UNKNOWN);
        return buf;
    }
    now = tv->tv_sec;
    ms  = tv->tv_usec / 1000;

    if (unlikely(gmtime_r(&now, &tm) == NULL)) {
        strcpy(buf, STAMP_UNKNOWN);
        return buf;
    }
    
    lc_time = setlocale(LC_TIME, NULL);
    setlocale(LC_TIME, "C");

    d = buf;
    strftime(d, STAMP_SIZE, STAMP_FORMAT, &tm);
    d += STAMP_STRFLEN;
    
    d[0] = '.';
    d[1] = '0' + ms / 100; ms %= 100;
    d[2] = '0' + ms /  10; ms %=  10;
    d[3] = '0' + ms;
    d[4] = '\0';

    setlocale(LC_TIME, lc_time);
        
    return buf;

}


/********************
 * format_message
 ********************/
static int
format_message(context_t *ctx, int id,
               const char *file, int line, const char *func,
               char *buf, int bufsize, const char *format, va_list args)
{
#define CHECK_SPACE(need, got) do {                     \
        if ((got) < (need))                             \
            goto nospace;                               \
    } while (0)
    
#define TIMEVAL_DIFF(diff, now, prev) do {                        \
        diff.tv_sec = now.tv_sec - prev.tv_sec;                   \
        if (now.tv_usec >= prev.tv_usec)                          \
            diff.tv_usec = now.tv_usec - prev.tv_usec;            \
        else {                                                    \
            diff.tv_sec--;                                        \
            diff.tv_usec = 1000000 - prev.tv_usec + now.tv_usec;  \
        }                                                         \
    } while (0)

    
    module_t *mod;
    flag_t   *flg;
    int       m, f;

    const char *s;
    char       *d, stamp[STAMP_SIZE];
    int         left, n, msg_printed;

    struct timeval diff, now;
    
    
    mod = NULL;
    flg = NULL;
    now.tv_sec = now.tv_usec = 0;
    msg_printed = FALSE;
    stamp[0] = '\0';

    s    = ctx->format;
    d    = buf;
    left = bufsize - 1;

    while (*s && left > 0) {
        
        if (*s != '%') {
            *d++ = *s++;
            left--;
            continue;
        }

        s++;

        /* hmm... should we add support for field width (ie. %15c) ? */

        switch (*s) {
        case 'c':                                           /* context name */
            n = snprintf(d, left, "%s", ctx->name);
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;

        case 'm':                                            /* module name */
            m   = FLAG_MOD(id);
            mod = MODULE_LOOKUP(ctx, m);
            n   = snprintf(d, left, "%s", mod ? mod->name : "<unknown>");
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;

        case 'f':                                              /* flag name */
            if (mod == NULL) {
                m   = FLAG_MOD(id);
                mod = MODULE_LOOKUP(ctx, m);
            }
            f   = FLAG_IDX(id);
            flg = FLAG_LOOKUP(mod, f);
            n   = snprintf(d, left, "%s", flg ? flg->name : "<unknown>");
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;

        case 'W':                         /* __FUNCTION__@__FILE__:__LINE__ */
            n = snprintf(d, left, "%s@%s:%d", func, file, line);
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;

        case 'C':                                           /* __FUNCTION__ */
            n = snprintf(d, left, "%s", func);
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;

        case 'F':                                               /* __FILE__ */
            n = snprintf(d, left, "%s", file);
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;

        case 'L':                                               /* __LINE__ */
            n = snprintf(d, left, "%d", line);
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;
            
        case 'U':                                /* absolute UTC time stamp */
            n = snprintf(d, left, "%s", get_timestamp(stamp, &now));
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;

        case 'u':                                   /* delta UTC time stamp */
            if (!ctx->prev.tv_sec) {
                n = snprintf(d, left, "%s",
                             stamp[0] ? stamp : get_timestamp(stamp, &now));
            }
            else {
                if (!now.tv_sec)
                    gettimeofday(&now, NULL);
                TIMEVAL_DIFF(diff, now, ctx->prev);
                n = snprintf(d, left, "+%4.4d.%3.3d",
                             (int)diff.tv_sec, (int)diff.tv_usec % 1000);
            }
            ctx->prev = now;
            CHECK_SPACE(n, left);
            d    += n;
            left -= n;
            break;
        
        case 'M':                                  /* user supplied message */
            n = vsnprintf(d, left, format, args);
            CHECK_SPACE(n, left);
            d    += (n - 1);                       /* chop off trailing '\n' */
            left -= (n - 1);
            msg_printed = TRUE;
            break;

        default:
            *d++ = *s;
            left--;
        }

        s++;
    }
    
    if (!msg_printed) {
        n = vsnprintf(d, left, format, args);
        CHECK_SPACE(n, left);
        d    += (n - 1);                           /* chop off trailing '\n' */
        left -= (n - 1);
    }
    
    if (left < 2)
        goto nospace;
    *d++ = '\n'; left--;
    *d   = '\0';
    
    return bufsize - left;
    

 nospace:
    /* in the case of overflow try to end the message with '...\0' */
    if (bufsize >= 4) {
        d = buf + bufsize - 1 - 3;
        d[0] = d[1] = d[2] = '.';
        d[3] = '\0';
    }
    else {
        *d = '\0';
    }
    return -EOVERFLOW;
}



/********************
 * check_format
 ********************/
static int
check_format(const char *format)
{
    const char *s = format;

    if (format == NULL || !*format)
        return -EILSEQ;

    while (*s) {
        if (*s != '%') {
            s++;
            continue;
        }

        s++;

        switch (*s) {
        case 'c':                                           /* context name */
        case 'm':                                            /* module name */
        case 'f':                                              /* flag name */
        case 'W':                         /* __FUNCTION__@__FILE__:__LINE__ */
        case 'C':                                           /* __FUNCTION__ */
        case 'F':                                               /* __FILE__ */
        case 'L':                                               /* __LINE__ */
        case 'U':                                /* absolute UTC time stamp */
        case 'u':                                   /* delta UTC time stamp */
        case 'M':                                  /* user supplied message */
            break;
        default:
            ERROR("Invalid format format string \"%s\".", format);
            ERROR("Illegal part detected at \"%s\".", s);
            return -EILSEQ;
        }
        
        s++;
    }
    
    return 0;
}


/*****************************************************************************
 *                     *** bitmap manipulation routines ***                  *
 *****************************************************************************/


/********************
 * alloc_bit
 ********************/
static inline int
alloc_bit(bitmap_t *bits)
{
    unsigned long *wptr, word;
    int            nw, i;
    
    if (bits->nbit <= BITS_PER_LONG) {
        wptr = &bits->bits.word;
        nw   = 1;
    }
    else {
        wptr = bits->bits.wptr;
        nw   = (bits->nbit + BITS_PER_LONG - 1) / BITS_PER_LONG;
    }
	
	
    /*
     * find word with free bit, then binary search within that word
     */
    
    for (i = 0, word = *wptr; i < nw && word == (-1UL); i++, word = *++wptr)
        ;

    if (i < nw) {
        i *= BITS_PER_LONG;
		
        /* XXX TODO: replace me with a BITS_PER_LONG-agnostic loop */
        if ((word & 0xffff) == 0xffff) i += 16, word >>= 16;
        if ((word & 0x00ff) == 0x00ff) i +=  8, word >>= 8;
        if ((word & 0x000f) == 0x000f) i +=  4, word >>= 4;
        if ((word & 0x0003) == 0x0003) i +=  2, word >>= 2;
        if ( word & 0x0001)            i +=  1, word >>= 1;

        *wptr |= (0x1 << (i & (BITS_PER_LONG - 1)));
        
        return i;
    }

    return -1;
}


/********************
 * set_bit
 ********************/
static inline int
set_bit(bitmap_t *bits, int i)
{
    unsigned long *wptr;
    
    if (0 <= i && i < bits->nbit) {
        if (bits->nbit <= BITS_PER_LONG)
            wptr = &bits->bits.word;
        else
            wptr = &bits->bits.wptr[i / BITS_PER_LONG];
        *wptr |= 1 << (i & (BITS_PER_LONG - 1));
    
        return 0;
    }
    else
        return -EOVERFLOW;
}


/********************
 * clr_bit
 ********************/
static inline int
clr_bit(bitmap_t *bits, int i)
{
    unsigned long *wptr;
	
    if (0 <= i && i < bits->nbit) {
        if (bits->nbit <= BITS_PER_LONG)
            wptr = &bits->bits.word;
        else
            wptr = bits->bits.wptr + (i / BITS_PER_LONG);

        *wptr &= ~(0x1 << (i & (BITS_PER_LONG - 1)));
        return 0;
    }
    else
        return -EOVERFLOW;
}


/********************
 * tst_bit
 ********************/
static inline int
tst_bit(bitmap_t *bits, int i)
{
    unsigned long word;
	
    if (0 <= i && i < bits->nbit) {
        if (bits->nbit <= BITS_PER_LONG)
            word = bits->bits.word;
        else
            word = bits->bits.wptr[i / BITS_PER_LONG];
        
        return (word & (1 << (i & (BITS_PER_LONG - 1)))) != 0;
    }
    else
        return 0;
}


/********************
 * init_bits
 ********************/
static void
init_bits(bitmap_t *bits)
{
    bits->nbit = 32;
}


/********************
 * realloc_bits
 ********************/
static int
realloc_bits(bitmap_t *bits, int n)
{
    int            oldn, newn;
    unsigned long *wptr;
	
    if (unlikely(n == bits->nbit))
        return 0;
	
    oldn = (bits->nbit + BITS_PER_LONG - 1) / BITS_PER_LONG;
    newn = (n          + BITS_PER_LONG - 1) / BITS_PER_LONG;
        
    if (n > bits->nbit) {                               /* grow */
        if (oldn == 1) {
            if ((wptr = ALLOC_ARR(typeof(*wptr), newn)) == NULL)
                return -ENOMEM;
            *wptr = bits->bits.word;
            bits->bits.wptr = wptr;
        }
        else {
            if (REALLOC_ARR(bits->bits.wptr, oldn, newn) == NULL)
                return -ENOMEM;
        }
    }
    else {                                            /* shrink */
        if (newn == 1) {
            wptr = bits->bits.wptr;
            bits->bits.word = *wptr;
            FREE(wptr);
        }
        else {
            if (REALLOC_ARR(bits->bits.wptr, oldn, newn) == NULL)
                return -ENOMEM;
        }
    }
    
    bits->nbit = newn * BITS_PER_LONG;
    
    return 0;
}


/********************
 * free_bits
 ********************/
static void
free_bits(bitmap_t *bits)
{
    if (bits->nbit > BITS_PER_LONG) {
        free(bits->bits.wptr);
        bits->bits.word = 0;
        bits->nbit      = BITS_PER_LONG;
		
    }
}


/********************
 * alloc_flag
 ********************/
static inline int
alloc_flag(context_t *ctx)
{
    bitmap_t *bits = &ctx->bits;
    int       bit  = alloc_bit(bits);

    if (bit < 0) {
        realloc_bits(bits, bits->nbit + 1);
        bit = alloc_bit(bits);
    }

    return bit;
}




/*****************************************************************************
 *                    *** configuration command parsing ***                  *
 *****************************************************************************/


/*
 * The possible commands are currently:
 *
 *    context.module=[+|-]flag1, ..., [+|-]flagn
 *    context > path, or context target path
 *    context format 'format'
 *    context enable
 *    context disable
 */


#define FLAG_ALL "all"
#define WILDCARD "*"
#define EQUAL    '='
#define MODSEP   '.'
#define FLAGSEP  ','
#define CMDSEP   ';'
#define ENABLE   "enabled"
#define DISABLE  "disable"
#define TARGET   "target"
#define REDIR    ">"
#define FORMAT   "format"


/********************
 * flip_flag
 ********************/
static int
flip_flag(char *context, char *module, char *flag)
{
    context_t *cptr;
    module_t  *mptr;
    flag_t    *fptr;
    int        nctx, nmod, nflg, off = FALSE;

    switch (flag[0]) {
    case '-': off = TRUE;
    case '+': flag++;
    }

    /* pick all or the named context */
    if (!strcmp(context, WILDCARD)) {
        cptr = contexts;
        nctx = ncontext;
    }
    else {
        if ((cptr = context_find(context, NULL)) == NULL) {
            ERROR("Context \"%s\" does not exist.", context);
            return -ENOENT;
        }
        nctx = 1;
    }

    for ( ; nctx > 0; cptr++, nctx--) {
        if (cptr->name == NULL)                    /* skip deleted contexts */
            continue;

        /* pick all or the named module */
        if (!strcmp(module, WILDCARD)) {
            mptr = cptr->modules;
            nmod = cptr->nmodule;
        }
        else {
            if ((mptr = module_find(cptr, module, NULL)) == NULL) {
                ERROR("Module \"%s.%s\" does not exist.", context, module);
                return - ENOENT;
            }
            nmod = 1;
        }

        
        for ( ; nmod > 0; mptr++, nmod--) {

            if (mptr->name == NULL)                /* skip deleted modules */
                continue;
        
            /* pick all or the named flag */
            if (!strcmp(flag, FLAG_ALL)) {
                fptr = mptr->flags;
                nflg = mptr->nflag;
            }
            else {
                if ((fptr = flag_find(mptr, flag, NULL)) == NULL) {
                    ERROR("Flag \"%s.%s.%s\" does not exist.", context,
                          module, flag);
                    return -ENOENT;
                }
                nflg = 1;
            }

            /* do the actual flag flipping */
            for ( ; nflg > 0; fptr++, nflg--) {
                if (fptr->name == NULL)            /* skip deleted flags */
                    continue;

                if (off) {
                    clr_bit(&cptr->mask, fptr->bit);
                    INFO("%s.%s.%s is now off.", cptr->name, mptr->name,
                               fptr->name);
                }
                else {
                    set_bit(&cptr->mask, fptr->bit);
                    INFO("%s.%s.%s is now on.", cptr->name, mptr->name,
                               fptr->name);
                }
            }
        }
    }

    return 0;
}


/********************
 * context_command
 ********************/
static int
context_command(char *context, char *command, char *args)
{
    context_t *cptr;
    int        nctx, status;
    char       arg[MAX_NAME];
    size_t     len;


    /* select all or only the named context */
    if (!strcmp(context, WILDCARD)) {
        cptr = contexts;
        nctx = ncontext;
    }
    else {
        if ((cptr = context_find(context, NULL)) == NULL) {
            ERROR("Context '%s' does not exist.", context);
            return -ENOENT;
        }
        nctx = 1;
    }

    
    /* command: "context enable" or "context disable" */
    if (!strcmp(command, DISABLE) || !strcmp(command, ENABLE)) {
        if (args != NULL && *args)
            WARNING("Ignoring extraneous argument '%s'.", args);

        for ( ; nctx > 0; cptr++, nctx--) {
            if (cptr->name == NULL)
                continue;
            cptr->disabled = (command[0] == 'd' ? TRUE : FALSE);
            INFO("%s is now %sabled.", context, command[0] == 'd' ? "dis":"en");
        }
        
        return 0;
    }
    
    /* command: "context target path" or "context > path" */
    if (!strcmp(command, TARGET) || !strcmp(command, REDIR)) {
        if (args != NULL && !args) {
            ERROR("Command target requires a path argument.");
            return -EILSEQ;
        }

        for (status = 0; nctx > 0; cptr++, nctx--) {
            if (cptr->name == NULL)
                continue;
            if (context_target(cptr, args) != 0) {
                ERROR("Failed to redirect '%s' to '%s'.", cptr->name, args);
                status = -EIO;
            }
            else
                INFO("'%s' redirected to '%s'.", cptr->name, args);
        }

        return status;
    }
    
    /* command: "context format 'format string for context'" */
    if (!strcmp(command, FORMAT)) {
        char *format;

        if ((len = strlen(args)) >= sizeof(arg) - 1) {
            ERROR("Format string '%s' too long for context '%s'.", args,
                  context);
            return -EILSEQ;
        }

        if (len > 2 && ((args[0] == '\'' && args[len-1] == '\'') ||
                        (args[0] == '"'  && args[len-1] == '"'))) {
            strncpy(arg, args + 1, len - 2);
            arg[len-2] = '\0';
            format = arg;
        }
        else
            format = args;
        
        for (status = 0; nctx > 0; cptr++, nctx--) {
            if (cptr->name == NULL)
                continue;
            if (context_format(cptr, format) != 0) {
                ERROR("Failed to set format %s for '%s'.", args, cptr->name);
                status = -EINVAL;
            }
            else
                INFO("Format for '%s' is now '%s'.", cptr->name, format);
        }

        return status;
    }


    ERROR("Unkown command '%s' for context '%s'.", command, context);
    return -EILSEQ;
}


/********************
 * context_configure
 ********************/
static const char *
context_configure(char *context, const char *config)
{
    char        module[MAX_NAME], flag[MAX_NAME];
    char        command[MAX_NAME], args[MAX_NAME];
    const char *s;
    char       *d;
    int         l;


    s = config;
    
    if (*s == MODSEP) {
        s++;
        
        d = module;
        l = 0;
        while (*s && *s != EQUAL && l < MAX_NAME - 1) {
            *d++ = *s++;
            l++;
        }
    
        if (*s != EQUAL) {
            ERROR("Expecting '%c' for context flag setting command.", EQUAL);
            goto invalid_input;
        }
        *d = '\0';
        s++;
        
        while (*s) {
            l = 0;
            d = flag;
            while (*s && *s != FLAGSEP && *s != CMDSEP && l < MAX_NAME - 1) {
                *d++ = *s++;
                l++;
            }

            if (*s && *s != FLAGSEP && *s != CMDSEP) {
                ERROR("Expecting either '%c' or '%c' after flag setting.",
                      FLAGSEP, CMDSEP);
                goto invalid_input;
            }
            *d = '\0';
            
            flip_flag(context, module, flag);
            
            if (*s == CMDSEP || !*s)
                return s;
            /* else if (*s == FLAGSEP) */
            s++;
        }
    }
    else if (*s == ' ') {
        while (*s == ' ')
            s++;
        
        d = command;
        l = 0;
        while (*s && *s != ' ' && *s != CMDSEP && l < MAX_NAME - 1) {
            *d++ = *s++;
            l++;
        }
        
        if (*s && *s != ' ' && *s != CMDSEP)
            goto invalid_input;
        *d = '\0';

        while (*s && *s == ' ')
            s++;
        
        d = args;
        l = 0;
        while (*s && *s != CMDSEP) {
            *d++ = *s++;
            l++;
        }
        
        if (*s && *s != CMDSEP) {
            ERROR("Context commands must be terminated by '%c'.", CMDSEP);
            goto invalid_input;
        }
        
        *d = '\0';

        context_command(context, command, args);
        return s;
    }
    

 invalid_input:
    ERROR("Invalid command '%s' for context '%s'.", config, context);
    return NULL;

}


/********************
 * trace_configure
 ********************/
int
trace_configure(const char *config)
{
    char        context[MAX_NAME];
    const char *s;
    char       *d;
    int         l;
    
    
    if (config == NULL)
        return -EINVAL;
    
    s = config;

    while (s != NULL && *s) {
        d = context;
        l = 0;
        while ((('a' <= *s && *s <= 'z') || ('A' <= *s && *s <= 'Z') ||
                ('0' <= *s && *s <= '9') || (*s == '_' || *s == '_') ||
                *s == '*') && l < MAX_NAME - 1) {
            *d++ = *s++;
            l++;
        }
        *d = '\0';
        
        if ((s = context_configure(context, s)) == NULL)
            return -1;
        
        if (*s == CMDSEP)
            s++;
    }

    return 0;
}


/********************
 * trace_show
 ********************/
int
trace_show(char *context, char *buf, size_t bufsize, const char *format)
{
    context_t *c;
    module_t  *m;
    flag_t    *f;
    int        nc, nm, nf, on;
    
    /* XXX FIXME temporarily just dump to stdout */
    (void)buf;
    (void)bufsize;
    (void)format;

    for (nc = 0, c = contexts; nc < ncontext; nc++, c++) {
        if (c->name == NULL)
            continue;
        
        for (nm = 0, m = c->modules; nm < c->nmodule; nm++, m++) {
            if (m->name == NULL)
                continue;
            
            for (nf = 0, f = m->flags; nf < m->nflag; nf++, f++) {
                if (f->name == NULL)
                    continue;
                
                on = tst_bit(&c->mask, f->bit);
                printf("%s.%s=%c%s\n", c->name, m->name, on ? '+':'-', f->name);
            }
        }
    }

    return 0;
}





/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */



