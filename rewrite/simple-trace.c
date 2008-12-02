#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "simple-trace.h"


#define BITS_PER_INT   ((int)sizeof(int) * 8)
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

#define CONTEXT_LOOKUP(cid) ({                  \
    trace_context_t *_ctx;                      \
    if (unlikely(cid < 0 || cid >= ncontext))   \
        _ctx = NULL;                            \
    else                                        \
        _ctx = contexts[cid].name ?             \
            contexts + (cid) : NULL;            \
    _ctx;})

#define MODULE_LOOKUP(ctx, id) ({                       \
    trace_module_t *_m;                                 \
    if ((ctx) != NULL) {                                \
        if (0 <= (id) && (id) < (ctx)->nmodule)         \
            _m = (ctx)->modules[id].name ?              \
                (ctx)->modules + (id) : NULL;           \
        else                                            \
            _m = NULL;                                  \
    }                                                   \
    else                                                \
        _m = NULL;                                      \
    _m;})

#define FLAG_LOOKUP(mod, idx) ({                        \
    trace_flag_t *_f;                                   \
    if ((mod) != NULL) {                                \
        if (0 <= (idx) && (idx) < (mod)->nflag)         \
            _f = (mod)->flags + (idx);                  \
        else                                            \
            _f = NULL;                                  \
    }                                                   \
    else                                                \
        _f = NULL;                                      \
    _f;})

#define TRACE_WARNING(fmt, args...) do {                                 \
        fprintf(stderr, "[WARNING] %s: "fmt"\n", __FUNCTION__, ## args); \
        fflush(stderr);                                                  \
    } while (0)

#define TRACE_ERROR(fmt, args...) do {                                   \
        fprintf(stderr, "[ERROR] %s: "fmt"\n", __FUNCTION__, ## args);   \
        fflush(stderr);                                                  \
    } while (0)


static trace_context_t *contexts;
static int              ncontext;
static char            *default_header = "[%C]";


static trace_context_t *context_find(const char *name,
                                     trace_context_t **deleted);
static void             context_del (trace_context_t *ctx);

static trace_module_t *module_find(trace_context_t *ctx, const char *name,
                                   trace_module_t **deleted);
static void module_free(trace_context_t *ctx, trace_module_t *module);

static inline int alloc_flag(trace_context_t *ctx);
static inline int free_flag (trace_context_t *ctx, int flag);
static void       init_bits (trace_bits_t *bits);
static void       free_bits (trace_bits_t *bits);

static inline int clr_bit(trace_bits_t *tb, int n);
static inline int set_bit(trace_bits_t *tb, int n);
static inline int tst_bit(trace_bits_t *tb, int n);




/********************
 * trace_init
 ********************/
int
trace_init(void)
{
    if (contexts != NULL)
        return 0;

    ncontext = 0;
    return 0;
}


/********************
 * trace_exit
 ********************/
void
trace_exit(void)
{
    trace_context_t *c;
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
 * trace_context_add
 ********************/
int
trace_context_add(const char *name)
{
    trace_context_t *ctx, *deleted;

    if ((ctx = context_find(name, &deleted)) != NULL)
        return ctx->id;

    if (deleted == NULL) {
        if (ncontext >= MAX_CONTEXTS) {
            errno = ENOSPC;
            return -1;
        }
        if (REALLOC_ARR(contexts, ncontext, ncontext + 1) == NULL)
            goto nomem;
        ctx = contexts + ncontext;
        ctx->id = ncontext++;
    }
    else
        ctx = deleted;

    if ((ctx->name = STRDUP(name)) == NULL)
        goto nomem;
    
    ctx->header      = default_header;
    ctx->destination = (FILE *)TRACE_TO_STDERR;

    init_bits(&ctx->bits);
    init_bits(&ctx->mask);

    return ctx->id;
    
    
 nomem:
    if (ctx) {
        FREE(ctx->name);
        FREE(ctx->modules);
    }
    errno = ENOMEM;
    return -1;
}


/********************
 * trace_context_del
 ********************/
int
trace_context_del(int cid)
{
    trace_context_t *ctx = CONTEXT_LOOKUP(cid);
    
    if (ctx == NULL) {
        errno = ENOENT;
        return -1;
    }

    context_del(ctx);
    if (cid == ncontext - 1) {
        if (ncontext > 1)
            REALLOC_ARR(contexts, ncontext, ncontext - 1);
        else
            FREE(contexts);
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
    trace_context_t *ctx = CONTEXT_LOOKUP(cid);

    if (ctx == NULL) {
        errno = ENOMEM;
        return -1;
    }

    ctx->disabled = FALSE;
    return 0;
}


/********************
 * trace_context_disable
 ********************/
int
trace_context_disable(int cid)
{
    trace_context_t *ctx = CONTEXT_LOOKUP(cid);

    if (ctx == NULL) {
        errno = ENOMEM;
        return -1;
    }

    ctx->disabled = TRUE;
    return 0;
}


/********************
 * trace_flag_set
 ********************/
int
trace_flag_set(int id)
{
    trace_context_t *ctx;
    trace_module_t  *mod;
    trace_flag_t    *flg;
    int              c, m, i, b;
    
    c = FLAG_CTX(id);
    m = FLAG_MOD(id);
    i = FLAG_IDX(id);
    b = FLAG_BIT(id);
    
    ctx = CONTEXT_LOOKUP(c);
    mod = MODULE_LOOKUP(ctx, m);
    flg = FLAG_LOOKUP(mod, i);

    if (unlikely(flg == NULL)) {
        errno = ENOENT;
        return -1;
    }

    if (unlikely(flg->bit != b)) {
        errno = EINVAL;
        return -1;
    }
    
    return set_bit(&ctx->bits, flg->bit);
}


/********************
 * trace_flag_clr
 ********************/
int
trace_flag_clr(int id)
{
    trace_context_t *ctx;
    trace_module_t  *mod;
    trace_flag_t    *flg;
    int              c, m, i, b;
    
    c = FLAG_CTX(id);
    m = FLAG_MOD(id);
    i = FLAG_IDX(id);
    b = FLAG_BIT(id);

    ctx = CONTEXT_LOOKUP(c);
    mod = MODULE_LOOKUP(ctx, m);
    flg = FLAG_LOOKUP(mod, i);

    if (unlikely(flg == NULL)) {
        errno = ENOENT;
        return -1;
    }
    
    if (unlikely(flg->bit != b)) {
        errno = EINVAL;
        return -1;
    }
        

    return clr_bit(&ctx->bits, flg->bit);
}


/********************
 * trace_flag_tst
 ********************/
int
trace_flag_tst(int id)
{
    trace_context_t *ctx;
    trace_module_t  *mod;
    trace_flag_t    *flg;
    int              c, m, i, b;
    
    c = FLAG_CTX(id);
    m = FLAG_MOD(id);
    i = FLAG_IDX(id);
    b = FLAG_BIT(id);

    ctx = CONTEXT_LOOKUP(c);
    mod = MODULE_LOOKUP(ctx, m);
    flg = FLAG_LOOKUP(mod, i);

    if (unlikely(flg == NULL)) {
        errno = ENOENT;
        return 0;
    }
    
    if (unlikely(flg->bit != b)) {
        errno = EINVAL;
        return 0;
    }
        
    return tst_bit(&ctx->bits, flg->bit);
}


/********************
 * trace_write
 ********************/
void
trace_write(int id, const char *format, ...)
{
    int              cid = FLAG_CTX(id);
    trace_context_t *ctx = CONTEXT_LOOKUP(cid);
    va_list          ap;
    
    if (ctx == NULL || ctx->disabled || !trace_flag_tst(id))
        return;
    
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    fflush(stdout);
    va_end(ap);
}


/********************
 * context_del
 ********************/
static void
context_del(trace_context_t *ctx)
{
    int i;

    FREE(ctx->name);
    ctx->name = NULL;

    if (ctx->header != default_header)
        FREE(ctx->header);
    ctx->header = NULL;
    
    if (ctx->destination != (FILE *)TRACE_TO_STDERR &&
        ctx->destination != (FILE *)TRACE_TO_STDOUT) {
        fflush(ctx->destination);
        fclose(ctx->destination);
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
static trace_context_t *
context_find(const char *name, trace_context_t **deleted)
{
    trace_context_t *ctx;
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

    errno = ENOENT;
    return NULL;
}


/********************
 * trace_module_add
 ********************/
int
trace_module_add(int cid, trace_module_t *module)
{
    trace_context_t *ctx = CONTEXT_LOOKUP(cid);
    trace_module_t  *m, *deleted;
    trace_flag_t    *mf, *f;
    int              i, nflag, n;

    if (ctx == NULL)
        return -1;

    if (module->name == NULL) {
        TRACE_WARNING("Module with NULL name for context %s.", ctx->name);
        errno = EINVAL;
        return -1;
    }
    
    if (module_find(ctx, module->name, &deleted) != NULL) {
        TRACE_WARNING("Context %s already has a module %s.", ctx->name,
                      module->name);
        errno = EEXIST;
        return -1;
    }

    nflag = 0;
    for (i = 0, f = module->flags; i < module->nflag; i++, f++) {
        if (f->name == NULL || f->flagptr == NULL) {
            if (i == module->nflag - 1)  /* it's okay to NULL-terminate */
                continue;
            TRACE_WARNING("#%d flag of %s.%s is invalid.", i + 1, ctx->name,
                          module->name);
            errno = EINVAL;
            return -1;
        }
        nflag++;
    }

    if (deleted == NULL) {
        n = ctx->nmodule;
        if (REALLOC_ARR(ctx->modules, n, n + 1) == NULL) {
            errno = ENOMEM;
            return -1;
        }
        
        m = ctx->modules + ctx->nmodule;
        m->id = ctx->nmodule++;
    }
    else
        m = deleted;
    
    if ((m->name = STRDUP(module->name)) == NULL) {
        errno = ENOMEM;
        return -1;
    }
    
    if ((m->flags = ALLOC_ARR(typeof(*m->flags), nflag)) == NULL) {
        errno = ENOMEM;
        return -1;
    }
    
    m->nflag = nflag;

    /* save module and allocate flag bits */
    for (i = 0; i < m->nflag; i++) {
        mf = m->flags + i;
        f  = module->flags + i;
        if ((mf->name  = STRDUP(f->name))   == NULL ||
            (mf->descr = STRDUP(mf->descr)) == NULL ||
            (mf->bit   = f->bit = alloc_flag(ctx)) < 0) {
            module_free(ctx, m);
            errno = ENOMEM;
            return -1;
        }
        
        if (mf->bit >= MAX_FLAGS) {
            module_free(ctx, m);
            errno = EOVERFLOW;
            return -1;
        }
        
        *f->flagptr = FLAG_ID(ctx->id, m->id, i, f->bit);
        mf->flagptr = f->flagptr;
    }
    
    return 0;
}


/********************
 * trace_module_del
 ********************/
int
trace_module_del(int cid, const char *name)
{
    trace_context_t *ctx;
    trace_module_t  *module;

    if (name == NULL) {
        errno = EINVAL;
        return -1;
    }

    if ((ctx = CONTEXT_LOOKUP(cid)) == NULL) {
        errno = ENOENT;
        return -1;
    }
    
    if ((module = module_find(ctx, name, NULL)) == NULL)
        return -1;
    
    module_free(ctx, module);
    if (module->id == ctx->nmodule - 1) {
        if (ctx->nmodule > 1)
            REALLOC_ARR(ctx->modules, ctx->nmodule, ctx->nmodule - 1);
        else
            FREE(ctx->modules);
        ctx->nmodule--;
    }
    
    return 0;
}


/********************
 * module_find
 ********************/
static trace_module_t *
module_find(trace_context_t *ctx, const char *name, trace_module_t **deleted)
{
    trace_module_t *m;
    int             i;

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
    
    errno = ENOENT;
    return NULL;
}
 
 
/********************
 * module_free
 ********************/
static void
module_free(trace_context_t *ctx, trace_module_t *module)
{
    trace_flag_t *flag;
    int           i;

    FREE(module->name);
    module->name = NULL;

    for (i = 0, flag = module->flags; i < module->nflag; i++, flag++) {
        FREE(flag->name);
        FREE(flag->descr);
        flag->name  = NULL;
        flag->descr = NULL;
        if (ctx != NULL)
            free_flag(ctx, flag->bit);
    }
    FREE(module->flags);
    module->flags = NULL;
    module->nflag = 0;
}





/*****************************************************************************
 *                     *** bitmap manipulation routines ***                  *
 *****************************************************************************/


/********************
 * alloc_bit
 ********************/
static inline int
alloc_bit(trace_bits_t *bits)
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
 * free_bit
 ********************/
static int
free_bit(trace_bits_t *bits, int n)
{
    unsigned long *wptr;
    
    if (n < 0 || n > bits->nbit) {
        errno = ERANGE;
        return -1;
    }
    
    if (bits->nbit <= BITS_PER_LONG)
        wptr = &bits->bits.word;
    else
        wptr = bits->bits.wptr + (n / BITS_PER_LONG);
    
    *wptr &= ~(0x1 << (n & (BITS_PER_LONG - 1)));
    return 0;
}


/********************
 * set_bit
 ********************/
static inline int
set_bit(trace_bits_t *bits, int i)
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
    else {
        errno = EOVERFLOW;
        return -1;
    }
}


/********************
 * clr_bit
 ********************/
static inline int
clr_bit(trace_bits_t *bits, int i)
{
    unsigned long *wptr;
	
    if (0 <= i && i < bits->nbit) {
        if (bits->nbit <= BITS_PER_LONG)
            wptr = &bits->bits.word;
        else
            wptr = &bits->bits.wptr[i / BITS_PER_LONG];
        *wptr &= ~(1 << (i & (BITS_PER_LONG - 1)));
        
        return 0;
    }
    else {
        errno = EOVERFLOW;
        return -1;
    }
}


/********************
 * tst_bit
 ********************/
static inline int
tst_bit(trace_bits_t *bits, int i)
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
init_bits(trace_bits_t *bits)
{
    bits->nbit = 32;
}


/********************
 * realloc_bits
 ********************/
static int
realloc_bits(trace_bits_t *bits, int n)
{
    int            oldn, newn;
    unsigned long *wptr;
	
    if (unlikely(n == bits->nbit))
        return 0;
	
    oldn = (bits->nbit + BITS_PER_LONG - 1) / BITS_PER_LONG;
    newn = (n          + BITS_PER_LONG - 1) / BITS_PER_LONG;
        
    if (n > bits->nbit) {                               /* grow */
        if (oldn == 1) {
            if ((wptr = ALLOC_ARR(typeof(*wptr), newn)) == NULL) {
                errno = ENOMEM;
                return -1;
            }
            *wptr = bits->bits.word;
            bits->bits.wptr = wptr;
        }
        else {
            if (REALLOC_ARR(bits->bits.wptr, oldn, newn) == NULL) {
                errno = ENOMEM;
                return -1;
            }
        }
    }
    else {                                            /* shrink */
        if (newn == 1) {
            wptr = bits->bits.wptr;
            bits->bits.word = *wptr;
            FREE(wptr);
        }
        else {
            if (REALLOC_ARR(bits->bits.wptr, oldn, newn) == NULL) {
                errno = ENOMEM;
                return -1;
            }
            bits->bits.wptr = wptr;
        }
    }
    
    bits->nbit = newn * BITS_PER_LONG;
    
    return 0;
}


/********************
 * free_bits
 ********************/
static void
free_bits(trace_bits_t *bits)
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
alloc_flag(trace_context_t *ctx)
{
    trace_bits_t *bits = &ctx->bits;
    int           bit  = alloc_bit(bits);

    if (bit < 0) {
        realloc_bits(bits, bits->nbit + 1);
        bit = alloc_bit(bits);
    }

    return bit;
}


/********************
 * free_flag
 ********************/
static inline int
free_flag(trace_context_t *ctx, int flag)
{
    return free_bit(&ctx->bits, flag);
}



/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */



