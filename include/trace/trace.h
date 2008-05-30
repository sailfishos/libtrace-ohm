#ifndef TRACE_TRACE_H_INCLUDED
#define TRACE_TRACE_H_INCLUDED

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/time.h>


#include <trace/list.h>

/* branch-prediction hint macros (same as in the linux kernel) */
#ifndef likely
#  ifdef __GNUC__
#    define likely(cond) __builtin_expect(cond, 1)
#  endif
#endif
#ifndef unlikely
#  ifdef __GNUC__
#    define unlikely(cond) __builtin_expect(cond, 0)
#  endif
#endif


/*
 * a single trace flag
 */

typedef struct trace_flag {
	char *name;                                    /* flag name */
	char *description;                             /* flag description */
	int   bit;                                     /* flag bit */
	int  *bitret;                                  /*   reported to caller */
} trace_flag_t;

/* initialize a flag */
#define TRACE_FLAG_INIT(n, d, br) {	\
		.name        = (n),			\
		.description = (d),		    \
		.bit         = -1,		    \
		.bitret      = (br)			\
}

/* flag array terminator/sentinel */
#define TRACE_FLAG_LAST TRACE_FLAG_INIT(NULL, NULL, NULL)

/* declare an array of flags */
#define TRACE_DECLARE_FLAGS(f, ...)  \
	trace_flag_t f[] = {			 \
		__VA_ARGS__,				 \
		TRACE_FLAG_LAST				 \
	}


/*
 * a traceable component
 */

typedef struct trace_component {
	list_hook_t   hook;                            /* to more components */
	char         *name;                            /* of this component */
	trace_flag_t *flags;                           /* trace flags */
} trace_component_t;


#define TRACE_COMPONENT_INIT(n, f)	{ \
		.name  = (n),				  \
		.flags = (f)				  \
	}

#define TRACE_DECLARE_COMPONENT(c, n, f) \
	trace_component_t c = TRACE_COMPONENT_INIT(n, f)


typedef struct trace_bits {
	union {
		unsigned long  word;                /* < 33 and dense bits */
		unsigned long *bits;                /* > 32 or sparse bits */
	} u;
	size_t             nbit;                /* total (used + unused) bits */
} trace_bits_t;


/*
 * a trace context
 */

typedef struct trace_context {
	list_hook_t         hook;                /* to more contexts */
	char               *name;                /* context name */
	trace_bits_t        bits;                /* allocated tracing bits */
	trace_bits_t        mask;                /* enabled trace flags */
	int                 enabled;             /* global tracing state */
	list_hook_t         components;          /* registered components */
	trace_component_t **flag_map;            /* flag -> component reverse map */
	FILE               *destination;         /* trace messages go here */
	const char         *header;              /* header format */
	struct timeval      last;                /* timestamp of last message */
	list_hook_t         simple_filters;      /* simple trace filters */
	list_hook_t         regexp_filters;      /* regexp trace filters */
	int                 filter_flags;        /* misc. filtering flags */
} trace_context_t;


#define TRACE_TO_STDERR     ((const char *)0)
#define TRACE_TO_STDOUT     ((const char *)1)
#define TRACE_TO_FILE(path) (path)

#define TRACE_HEADER_FORMAT "=====\n%D %i:\nlocation: %W\nkeywords: %T\n-----"
#define TRACE_FLAG_FORMAT    "  %c.%f:\t%d"   /* default listing format */
#define TRACE_FLAG_SEPARATOR "\n"             /* and separator */



/*
 * trace tags
 */

typedef struct trace_tag {
	const char    *key;                          /* tag key */
	const char    *value;                        /* tag value */
	unsigned long  hash;                         /* filtering hash */
} trace_tag_t;

/* no trace tags */
#define TRACE_NO_TAGS (NULL)


/*
 * a set of trace tags
 */

typedef struct trace_tags {
	trace_tag_t   *tags;                         /* the actual tags */
	size_t         tsize;                        /* # of possible tags */
	size_t         tused;                        /* # of tags in use */
	unsigned long  hash;                         /* hash of tags */
	char          *buf;                          /* value buffer */
	size_t         bsize;                        /* value buffer size */
} trace_tags_t;

/* convenience macro to allocate a set of tags and a value buffer */
#define TRACE_DECLARE_TAGS(t, maxtags, maxbuf)							\
	trace_tag_t   __##t##_tags[(maxtags+1)];							\
	char          __##t##_buf[(maxbuf+1)] = { [(maxbuf)] = '\0' };		\
	trace_tags_t t = {													\
		.hash    = (unsigned long)-1,									\
		.tags    = &__##t##_tags[0],									\
		.tsize   = maxtags,												\
		.tused   = 0,													\
		.buf     = __##t##_buf,											\
		.bsize   = maxbuf,												\
	}


/* add a printf-formatted tag to a set of tags */
#define trace_tag_addf(ts, k, fmt, args...) ({							\
			trace_tag_t *__t;											\
			int          __n, __status;									\
																		\
			if (likely((ts)->tused < (ts)->tsize)) {					\
				__t = (ts)->tags + (ts)->tused;							\
				__t->key   = k;											\
				__t->value = (ts)->buf;									\
				(__t + 1)->key = NULL;									\
				__n = snprintf((char *)__t->value, (ts)->bsize, fmt, ## args); \
				if (likely(__n <= (ts)->bsize)) {						\
					(ts)->buf   += (__n + 1);							\
					(ts)->bsize -= (__n + 1);							\
					(ts)->tused++;										\
					__status = 0;										\
				}														\
				else													\
					__status = ENOSPC;									\
			}															\
			else														\
				__status = EOVERFLOW;									\
			__status; })

/* add a preformatted tag to a set of tags */
#define trace_tag_add(ts, k, v) ({							\
			trace_tag_t *__t;								\
			int          __status;							\
															\
			if (likely((ts)->tused < (ts)->tsize)) {		\
				__t = (ts)->tags + (ts)->tused++;			\
				(__t + 1)->key = NULL;						\
				__t->key   = k;								\
				__t->value = v;								\
				__status = 0;								\
				(ts)->tags[(ts)->tused].key = NULL;			\
			}												\
			else											\
				__status = EOVERFLOW;						\
			__status; })


/*
 * trace filters
 */

#define TRACE_FILTER_NO_TAGS    "empty"        /* filter to match empty tags */
#define TRACE_FILTER_ALL_TAGS   "all"          /* filter to match all tags */

enum {
	TRACE_SIMPLE_FILTER = 1,
	TRACE_REGEXP_FILTER
};


/* simple filters */
#define TRACE_SIMPLE_FILTER_SIZE(n) \
	(sizeof(trace_simple_filter_t) + (n)*sizeof(trace_simple_tag_t))

typedef struct trace_simple_tag {
	const char    *key;                        /* filter key */
	const char    *value;                      /* filter value */
	unsigned long  hash;                       /* filter hash */
} trace_simple_tag_t;

typedef struct trace_simple_filter {
	list_hook_t        hook;                   /* to more filters */
	int                type;                   /* TRACE_SIMPLE_FILTER */
	unsigned long      hash;                   /* filter hash */
	int                ntag;                   /* number of tags */
	trace_simple_tag_t tags[0];                /* filter tags */
} trace_simple_filter_t;


/* regexp filters */
#define TRACE_REGEXP_FILTER_SIZE(n) \
    (sizeof(trace_regexp_filter_t) + (n) * sizeof(trace_regexp_tag_t))

typedef struct trace_regexp_tag {
	const char *key;                           /* filter key */
	const char *value;                         /* filter value regexp */
	regex_t     regex;                         /* compiled regexp */
} trace_regexp_tag_t;

typedef struct trace_regexp_filter {
	list_hook_t        hook;                   /* to more filters */
	int                type;                   /* TRACE_FILTER_REGEXP */
	int                ntag;                   /* number of tags */
	trace_regexp_tag_t tags[0];                /* array of tags */
} trace_regexp_filter_t;



/********************
 * trace_hash_tags
 ********************/
static inline unsigned long
trace_hash_tags(trace_tag_t *tags)
{
	unsigned long  hash, h, c;
	int            ntag, ki, vi;
	unsigned char *k, *v;

	
	/*
	 * XXX TODO: Blow this apart to get something more managable. Look
	 *           at the misuse in simple-filter.c to see what I mean...
	 */

	hash = 0;
	for (ntag = 0; tags->key; tags++, ntag++) {
		k = (unsigned char *)tags->key;
		v = (unsigned char *)tags->value;
		for (h = ki = vi = 0; *k || *v; h += c) {
			c = 0;
			if (*k)
				c ^= (*k++ << ki++);
			if (*v)
				c ^= (*v++ << vi++);
		}
		c = (c & 0xffffUL) ^ (c >> 16);
		tags->hash = h;
		hash += h;
#if 0
		printf("*** hash(%s=%s): 0x%lx\n", tags->key, tags->value, h);
#endif
	}
	hash &= ~0xf0000000;
	hash |= (ntag << 28);
#if 0
	printf("*** tagset hash: 0x%lx\n", hash);
#endif
	
	return hash;
}



/*****************************************************************************
 *                           *** public trace API ***                        *
 *****************************************************************************/


int  trace_init(void);
void trace_exit(void);

int  trace_open(trace_context_t *, char *);
void trace_close(trace_context_t *tc);
trace_context_t *trace_find_context(const char *name);


int  trace_add_component(trace_context_t *, trace_component_t *);
int  trace_del_component(trace_context_t *, const char *);

int  trace_list_flags(trace_context_t *, char *, size_t, char *, char *);

int  trace_set_destination(trace_context_t *, const char *);
int  trace_set_header(trace_context_t *, const char *);

int  trace_enable(trace_context_t *);
int  trace_disable(trace_context_t *);

int  trace_off(trace_context_t *, int);
int  trace_on(trace_context_t *, int);

void __trace_write(trace_context_t *tc,
				   const char *file, int line, const char *function,
				   int flag, trace_tags_t *tags, char *format, ...);
void
__trace_writel(trace_context_t *tc,
			   const char *file, int line, const char *function,
			   int flag, trace_tags_t *tags, char *format, va_list ap);


#define STRINGIFY(x) #x

#define trace_write(c, f, tags, format, args...) do {					\
		if ((c)->enabled)												\
			__trace_write(c, __FILE__, __LINE__, __FUNCTION__,			\
						  f, tags, format, ## args);					\
	} while (0)


void trace_reset_filters(trace_context_t *tc);
int trace_add_simple_filter(trace_context_t *tc, char *);
int trace_del_simple_filter(trace_context_t *tc, char *);
int trace_add_regexp_filter(trace_context_t *tc, char *);
int trace_del_regexp_filter(trace_context_t *tc, char *);

int trace_add_regexp_filter(trace_context_t *tc, char *);

char *trace_print_tags(trace_tag_t *tags, char *buf, size_t size);


/* parser.c */
int trace_parse_flags(char *flags);
trace_tag_t *trace_parse_filter(char *filter);



/* simple-filter.c */
trace_simple_filter_t *simple_alloc(trace_context_t *tc, int ntag,
									trace_tag_t *tags);
void simple_free(trace_context_t *tc, trace_simple_filter_t *f);
int  simple_identical(trace_simple_filter_t *f1, trace_simple_filter_t *f2);

int  simple_compare(trace_simple_filter_t *filter, trace_tag_t *tags);
int  simple_match_tags(trace_context_t *tc, trace_tag_t *tags);

/* regexp-filter.c */
trace_regexp_filter_t *regexp_alloc(trace_context_t *tc, int ntag,
									trace_tag_t *tags);
void regexp_free (trace_context_t *tc, trace_regexp_filter_t *f);
int  regexp_identical(trace_regexp_filter_t *f1, trace_regexp_filter_t *f2);
int  regexp_compare(trace_regexp_filter_t *filter, trace_tag_t *tags);
int  regexp_match_tags(trace_context_t *tc, trace_tag_t *tags);
char *regexp_print(trace_regexp_filter_t *filter, char *buf, size_t size);





#endif /* TRACE_TRACE_H_INCLUDED */



/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */

