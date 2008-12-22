#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <trace/list.h>
#include <trace/trace.h>

#undef __TRACE_FORCE_NEWLINE__       /* don't force a \n after the header */

#define UTC_STRFTIME    "%Y-%m-%d %H:%M:%S"
#define UTC_STRFLEN     (4+1+2+1+2+1+2+1+2+1+2)       /* see above */
#define UTC_MSECLEN     (1+3)                         /* ".xyz" */
#define UTC_MIN_BUFSIZE (UTC_STRFLEN+UTC_MSECLEN+1)

#define CHECK_DEFAULT_CONTEXT(tc) do {			\
		if ((tc) == TRACE_DEFAULT_CONTEXT)		\
			(tc) = default_context();			\
	} while (0)


/*
 * private filtering flags
 */

enum {
	FILTER_NONE       = 0x0,
	FILTER_PASS_EMPTY = 0x1,                 /* pass messages without tags */
	FILTER_PASS_ALL   = 0x2,                 /* pass all messages through */
};


#define __E(fmt, args...) do {											\
		fprintf(stderr, "### => %s [%u] "fmt" ###\n",					\
				__FUNCTION__, getpid(), ## args);						\
		fflush(stderr);													\
	} while (0)


static trace_context_t __dc;


static int alloc_flags(trace_context_t *tc, trace_component_t *c);
static int free_flags (trace_context_t *tc, trace_component_t *c);

static int  realloc_bits(trace_bits_t *tb, int n);
static void free_bits(trace_bits_t *tb);

static inline const char *flag_name(trace_context_t *tc, int flag);
static inline const char *component_name(trace_context_t *tc, int flag);

static int format_header(trace_context_t *tc, char *buf, size_t size,
						 const char *file, int line, const char *function,
						 int flag, trace_tag_t *tags);
static int check_header(const char *format);

static void                    default_init(void);
static inline trace_context_t *default_context(void);


static LIST_HOOK(contexts);                 /* open trace contexts */
static int initialized = 0;


/*****************************************************************************
 *                       *** initialization & cleanup ***                    *
 *****************************************************************************/


/********************
 * trace_init
 ********************/
int
trace_init(void)
{
	if (!initialized) {
		list_hook_init(&contexts);
		initialized = 1;
	}
	
	return 0;
}


/********************
 * trace_exit
 ********************/
void
trace_exit(void)
{
	trace_context_t *tc;
	list_hook_t     *p, *n;

	list_iter_forw_safe(p, n, &contexts) {
		tc = list_entry(p, trace_context_t, hook);
		trace_close(tc);
	}

	if (__dc.name) {
		free(__dc.name);
		__dc.name = NULL;
	}

	initialized = 0;
}


/********************
 * trace_open
 ********************/
int
trace_open(trace_context_t *tc, char *name)
{
	memset(tc, 0, sizeof(*tc));

	tc->name        = strdup(name);
	tc->bits.nbit   = tc->mask.nbit = 32;
	list_hook_init(&tc->components);

	trace_set_header(tc, TRACE_HEADER_FORMAT);
	trace_set_destination(tc, TRACE_TO_STDOUT);
	trace_disable(tc);                              /* trace off by default */

	list_hook_init(&tc->simple_filters);
	list_hook_init(&tc->regexp_filters);

	list_add_tail(&tc->hook, &contexts);
	
	return 0;
}


/********************
 * trace_close
 ********************/
void
trace_close(trace_context_t *tc)
{
	list_hook_t           *p, *n;
	trace_component_t     *comp;
	trace_simple_filter_t *sf;
	trace_regexp_filter_t *rf;

	if (tc == &__dc)
		return;

	list_delete(&tc->hook);
	
	list_iter_forw_safe(p, n, &tc->components) {
		comp = list_entry(p, trace_component_t, hook);
		trace_del_component(tc, comp->name);
	}

	free_bits(&tc->bits);
	free_bits(&tc->mask);
	if (tc->flag_map)
		free(tc->flag_map);

	list_iter_forw_safe(p, n, &tc->simple_filters) {
		sf = list_entry(p, trace_simple_filter_t, hook);
		list_delete(&sf->hook);
		simple_free(tc, sf);
	}

	list_iter_forw_safe(p, n, &tc->regexp_filters) {
		rf = list_entry(p, trace_regexp_filter_t, hook);
		list_delete(&rf->hook);
		regexp_free(tc, rf);
	}

	if (tc->name)
		free(tc->name);
	if (tc->header)
		free((char *)tc->header);
	tc->name   = NULL;
	tc->header = NULL;

	switch ((int)tc->destination) {
	case (int)TRACE_TO_STDERR:
	case (int)TRACE_TO_STDOUT:
		break;
	default:
		fclose(tc->destination);
	}
	
	return;
}


/********************
 * trace_find_context
 ********************/
trace_context_t *
trace_find_context(const char *name)
{
	trace_context_t *tc;
	list_hook_t     *p;

	if (__dc.name && (!name || !name[0] || !strcmp(name, "default")))
		return &__dc;
	
	list_iter_forw(p, &contexts) {
		tc = list_entry(p, trace_context_t, hook);
		if (!strcmp(tc->name, name))
			return tc;
	}

	
	return NULL;
}


/*****************************************************************************
 *                             *** configuration ***                         *
 *****************************************************************************/


/********************
 * trace_set_destination
 ********************/
int
trace_set_destination(trace_context_t *tc, const char *to)
{
	FILE *dfp, *old;
	int   status;

	CHECK_DEFAULT_CONTEXT(tc);
	
	status = 0;
	old    = tc->destination;
	
	if (to == TRACE_TO_STDERR)
		dfp = stderr;
	else if (to == TRACE_TO_STDOUT)
		dfp = stdout;
	else {
		if ((dfp = fopen(to, "a")) == NULL) {
			dfp    = stderr;
			status = errno;
		}
	}
	
	if (old && old != stdout && old != stderr)
		fclose(old);
	
	tc->destination = dfp;
	return status;
}


/********************
 * trace_set_header
 ********************/
int
trace_set_header(trace_context_t *tc, const char *header)
{
	const char *old;

	if (check_header(header))                 /* XXX implement me */
		return EINVAL;

	CHECK_DEFAULT_CONTEXT(tc);
		
	old = tc->header;
	tc->header = strdup(header);
	
	if (tc->header == NULL) {
		tc->header = old;
		return ENOMEM;
	}

	free((void *)old);
	return 0;
}


/*****************************************************************************
 *                         *** component registration ***                    *
 *****************************************************************************/

/********************
 * trace_add_component
 ********************/
int
trace_add_component(trace_context_t *tc, trace_component_t *c)
{
	trace_component_t **map;
	trace_flag_t       *f;
	int                 err;

	CHECK_DEFAULT_CONTEXT(tc);

	/*
	 * allocate trace flag values, active flag bitmask
	 */
	
	if ((err = alloc_flags(tc, c)) != 0)
		return err;
	
	if (realloc_bits(&tc->mask, tc->bits.nbit))
		return ENOMEM;
	

	/*
	 * allocate and initialize flag reverse (flag -> component) map
	 */

	if (tc->flag_map)
		map = realloc(tc->flag_map, tc->bits.nbit * sizeof(map[0]));
	else
		map = malloc(tc->bits.nbit * sizeof(map[0]));

	if (map == NULL)
		return ENOMEM;
	
	tc->flag_map = map;
	for (f = c->flags; f->name != NULL; f++)
		tc->flag_map[f->bit] = c;
	
	
	/*
	 * hook in among the components
	 */
	
	list_add_tail(&c->hook, &tc->components);


	return 0;
}


/********************
 * trace_del_component
 ********************/
int
trace_del_component(trace_context_t *tc, const char *name)
{
	trace_component_t *c;
	list_hook_t       *p;

	CHECK_DEFAULT_CONTEXT(tc);

	c = NULL;
	list_iter_forw(p, &tc->components) {
		c = list_entry(p, trace_component_t, hook);
		if (!strcmp(name, c->name)) {
			list_delete(&c->hook);
			free_flags(tc, c);
			return 0;
		}
	}
	
	return ENOENT;
}


/*****************************************************************************
 *                    *** bit(set) manipulation routines ***                 *
 *****************************************************************************/

/********************
 * set_bit
 ********************/
static inline int
set_bit(trace_bits_t *tb, int n)
{
	unsigned long *wp;
	
	if (n > tb->nbit || n < 0)
		return EOVERFLOW;
	
	if (tb->nbit <= 32)
		wp = &tb->u.word;
	else
		wp = &tb->u.bits[n / 32];
	*wp |= 1 << (n & 31);

	return 0;
}


/********************
 * clr_bit
 ********************/
static inline int
clr_bit(trace_bits_t *tb, int n)
{
	unsigned long *wp;
	
	if (n > tb->nbit || n < 0)
		return EOVERFLOW;
	
	if (tb->nbit <= 32)
		wp = &tb->u.word;
	else
		wp = &tb->u.bits[n / 32];
	*wp &= ~(1 << (n & 31));

	return 0;
}


/********************
 * tst_bit
 ********************/
static inline int
tst_bit(trace_bits_t *tb, int n)
{
	unsigned long *wp;
	
	if (n > tb->nbit || n < 0)
		return 0;
	
	if (tb->nbit <= 32)
		wp = &tb->u.word;
	else
		wp = &tb->u.bits[n / 32];

	return *wp & (1 << (n & 31));
}


/*****************************************************************************
 *                      *** flag manipulation/listing ***                    *
 *****************************************************************************/


/********************
 * trace_enable
 ********************/
int
trace_enable(trace_context_t *tc)
{
	int old;

	CHECK_DEFAULT_CONTEXT(tc);

	old = tc->enabled;
	tc->enabled = 1;

	return old;
}


/********************
 * trace_disable
 ********************/
int
trace_disable(trace_context_t *tc)
{
	int old;

	CHECK_DEFAULT_CONTEXT(tc);

	old = tc->enabled;
	tc->enabled = 0;
	
	return old;
}


/********************
 * trace_on
 ********************/
int
trace_on(trace_context_t *tc, int flag)
{
	CHECK_DEFAULT_CONTEXT(tc);
	
	if (!tst_bit(&tc->bits, flag))
		return EINVAL;

	set_bit(&tc->mask, flag);
	return 0;
}


/********************
 * trace_off
 ********************/
int
trace_off(trace_context_t *tc, int flag)
{
	CHECK_DEFAULT_CONTEXT(tc);

	if (!tst_bit(&tc->bits, flag))
		return EINVAL;
	
	clr_bit(&tc->mask, flag);
	return 0;
}


/********************
 * trace_list_flags
 ********************/
int
trace_list_flags(trace_context_t *tc, char *buf, size_t size,
				 char *format, char *separator)
{
#define OVERFLOW_CHECK(r, s) do { if ((r) >= (s)) goto overflow; } while (0)
#define NONE "<none>"

	trace_component_t *c;
	trace_flag_t      *f;
	list_hook_t       *p;
	const char        *s;
	char              *d, *item, *sep, *end, *fmt;
	char               fmtstr[64], itembuf[128];
	int                left, n, nl, sep_len, minw, maxw, justify;

	CHECK_DEFAULT_CONTEXT(tc);

	d    = buf;
	left = size;
	*d   = '\0';

	if (format == NULL)
		format = TRACE_FLAG_FORMAT;
	if (separator == NULL)
		separator = TRACE_FLAG_SEPARATOR;
	sep_len = strlen(separator);

	/*
	 * generate a list of flags according to format
	 */

	if (list_is_empty(&tc->components)) {
		s = format;
		/* try to indent '<none>' according to format */
		while ((*s == ' ' || *s == '\t') && left > 0) {
			*d++ = *s++;
			left--;
		}
		if (left < sizeof(NONE))
			goto overflow;
		for (; *s; s++)
			nl = (*s == '\n');
		
		strcpy(d, NONE);
		d    += sizeof(NONE) - 1;
		left -= sizeof(NONE) - 1;
		
		if (left > 1) {
			*d = '\0';
			return (int)(d - buf);
		}
		else {                                   /* try to protect fools... */
			if (d > buf)
				d[-1] = '\0';
			else
				*buf = '\0';
			return -EOVERFLOW;
		}
	}
	else {
		sep = "";
		list_iter_forw(p, &tc->components) {
			c = list_entry(p, trace_component_t, hook);
			for (f = c->flags; f->name != NULL; f++) {
				s = format;
				if (*sep) {
					OVERFLOW_CHECK(sep_len, left);
					strcpy(d, sep);
					d    += sep_len;
					left -= sep_len;
				}
				while (*s && left > 0) {

					if (*s != '%') {
						*d++ = *s++;
						left--;
						continue;
					}
					
					s++;
					
					if (*s == '-') {
						justify = -1;
						s++;
					}
					else
						justify = +1;
					
					minw = maxw = 0;
					if ('0' <= *s && *s <= '9') {
						minw = (int)strtol(s, &end, 10);
						if (end == NULL)
							goto formaterr;
						if (*end == '.') {
							s = end + 1;
							maxw = (int)strtol(s, &end, 10);
							if (end == NULL)
								goto formaterr;
						}
						s = end;
					}

					switch (*s) {
					case 'c': item = c->name;        goto emit;
					case 'f': item = f->name;        goto emit;
					case 'd': item = f->description; goto emit;
					case 'F':
						sprintf(item = itembuf, "%s.%s", c->name, f->name);
						                             goto emit;
					case 's': item = tst_bit(&tc->mask, f->bit) ? "on" : "off";
					emit:
						fmt = fmtstr;
						*fmt++ = '%';
						if (justify < 0)
							*fmt++ = '-';
						if (minw > 0)
							fmt += sprintf(fmt, "%d", minw);
						if (maxw > 0)
							fmt += sprintf(fmt, ".%d", maxw);
						*fmt++ = 's';
						*fmt   = '\0';
						n = snprintf(d, left, fmtstr, item);
						OVERFLOW_CHECK(n, left);
						d    += n;
						left -= n;
						s++;
						break;
					default:
						*d++ = *s++;
						left--;
					}
				}
				sep = separator;
			}
		}
	}
	
	if (left < 1)
		goto overflow;
			
	*d = '\0';
	return (d - buf);
	
 overflow:
	if (d > buf + 4 && size > 3) {
		d[-4] = d[-3] = d[-2] = '.';
		d[-1]  = '\0';
	}
	else { 
		if (d > buf)
			d[-1] = '\0';
		else
			*buf = '\0';
	}
	return -EOVERFLOW;

 formaterr:
	*buf = '\0';
	return -EINVAL;
}


/********************
 * suppress_message
 ********************/
static int
suppress_message(trace_context_t *tc, int flag, trace_tags_t *tags)
{
	if (!tst_bit(&tc->mask, flag))                        /* trace flag off */
		return 1;

	if (tc->filter_flags & FILTER_PASS_ALL)               /* pass all tags */
		return 0;

	if (tags != TRACE_NO_TAGS) {                          /* non-empty tags */
		trace_hash_tags(tags->tags);
		if (simple_match_tags(tc, tags->tags) ||
			regexp_match_tags(tc, tags->tags))
			return 0;
	}
	else                                                  /* empty tags */
		if (tc->filter_flags & FILTER_PASS_EMPTY)
			return 0;
	
	return 1;
}


/********************
 * __trace_write
 ********************/
void
__trace_write(trace_context_t *tc,
			  const char *file, int line, const char *function,
			  int flag, trace_tags_t *tags, char *format, ...)
{
	va_list ap;
	char    header[1024];

	CHECK_DEFAULT_CONTEXT(tc);

	if (suppress_message(tc, flag, tags))
		return;

	format_header(tc, header, sizeof(header),
				  file, line, function, flag, tags ? tags->tags : NULL);

	va_start(ap, format);
	fprintf(tc->destination, header);
	vfprintf(tc->destination, format, ap);
	fflush(tc->destination);
	va_end(ap);
}


/********************
 * __trace_writel
 ********************/
void
__trace_writel(trace_context_t *tc,
			   const char *file, int line, const char *function,
			   int flag, trace_tags_t *tags, char *format, va_list ap)
{
	char header[1024];

	CHECK_DEFAULT_CONTEXT(tc);
	
	if (suppress_message(tc, flag, tags))
		return;
	
	format_header(tc, header, sizeof(header),
				  file, line, function, flag, tags ? tags->tags : NULL);

	fprintf(tc->destination, header);
	vfprintf(tc->destination, format, ap);
	fflush(tc->destination);
}


/********************
 * current_utc_time
 ********************/
static char *
current_utc_time(char *buf)
{
	struct timeval now_tv;
	struct tm      now_tm;
	time_t         now;
	int            msec;
	char          *p;

	/*
	 * Notes:
	 *   Since this routine is called for every single trace message we want
	 *   to minimize the runtime overhead of it as much as possible. The
	 *   caller of this must call with a buffer size equal or greater to
	 *   UTC_MIN_BUFSIZE. As this routine is private to the trace library
	 *   we consider it safe to not pass in and check the buffer size.
	 */
	
	if (unlikely(gettimeofday(&now_tv, NULL) < 0))
		return "unknown time";
	now = now_tv.tv_sec;
	if (unlikely(gmtime_r(&now, &now_tm) == NULL))
		return "unknown time";
	
	strftime(buf, UTC_MIN_BUFSIZE, UTC_STRFTIME, &now_tm);
	
	p = buf + UTC_STRFLEN;
	msec = now_tv.tv_usec / 1000;
	*p++ = '.';
	*p++ = '0' + msec / 100; msec %= 100;
	*p++ = '0' + msec /  10; msec %=  10;
	*p++ = '0' + msec;
	*p = '\0';
	
	return buf;
}


/********************
 * format_header
 ********************/
static int
format_header(trace_context_t *tc, char *buf, size_t size,
			  const char *file, int line, const char *function,
			  int flag, trace_tag_t *tags)
{
#define OVERFLOW_CHECK(r, s) do { if ((r) >= (s)) goto overflow; } while (0)

	char            utc[UTC_MIN_BUFSIZE];
	const char     *s, *p;
	char           *d;
	int             n, left;
	struct timeval  now, diff;
	trace_tag_t    *t;

	s = tc->header;
	d = buf;
	left = size - 1;
	
	current_utc_time(utc);
	gettimeofday(&now, NULL);
	
	while (*s && left > 0) {
		
		if (*s != '%') {
			*d++ = *s++;
			left--;
			continue;
		}

		if (left <= 0)
			goto overflow;

		s++;
		
		switch (*s) {
		case 'i':                                   /* context id */
			n = snprintf(d, left, "%s", tc->name);
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;

		case 'D':                                   /* absolute UTC time */
			n = snprintf(d, left, utc);
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;
			
		case 'd':                                   /* delta time */
			if (!tc->last.tv_sec)
				n = snprintf(d, left, "%s", utc);
			else {
				int sec, usec;
				sec = now.tv_sec - tc->last.tv_sec;
				if (now.tv_usec >= tc->last.tv_usec)
					usec = now.tv_usec - tc->last.tv_usec;
				else {
					sec--;
					usec = 1000000 - tc->last.tv_usec + now.tv_usec;
				}
				
                n = snprintf(d, left, "+%4.4d.%3.3d", sec, usec % 1000);
			}
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;

		case 'T':                                   /* trace tags */
			if (tags && tags->key) {
				for (t = tags, p = ""; t->key; t++, p = ", ") {
					n = snprintf(d, left, "%s%s=%s", p, t->key, t->value);
					OVERFLOW_CHECK(n, left);
				    d += n;
				    left -= n;
				}
			}
			else {
				n = snprintf(d, left, "<no tags>");
				OVERFLOW_CHECK(n, left);
				d += n;
				left -= n;
			}
			s++;
			break;
				
		case 'c':                                   /* flag component name */
			n = snprintf(d, left, "%s", component_name(tc, flag));
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;		   
			
		case 'f':                                   /* flag name */
			n = snprintf(d, left, "%s", flag_name(tc, flag));
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;
			
		case 'W':                                   /* function@file:line */
			n = snprintf(d, left, "%s@%s:%d", function, file, line);
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;

		case 'C':                                   /* function */
			n = snprintf(d, left, "%s", function);
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;

		case 'F':                                   /* file */
			n = snprintf(d, left, "%s", file);
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;

		case 'L':                                   /* line */
			n = snprintf(d, left, "%d", line);
			OVERFLOW_CHECK(n, left);
			d += n;
			left -= n;
			s++;
			break;
			
		default:
			*d++ = *s++;
			left++;
			break;
		}
	}
	
	if (left < 2)
		goto overflow;
#ifdef __TRACE_FORCE_NEWLINE__
	if (d > buf && d[-1] != '\n')
		*d++ = '\n';
#else
	if (d > buf && d[-1] != ' ' && d[-1] != '\t')
		*d++ = ' ';
#endif
	*d = '\0';

	tc->last = now;
	return 0;

 overflow:
	if (size > 4) {
		d = buf + size - 1 - 4;
		d[0] = d[1] = d[2] = '.';
		d[3] = '\n';
		d[4] = '\0';
	}
	tc->last = now;
	return EOVERFLOW;

#undef OVERFLOW_CHECK
}


/********************
 * flag_name
 ********************/
static inline const char *
flag_name(trace_context_t *tc, int flag)
{
	trace_component_t *c;

	if (flag < 0 || flag >= tc->bits.nbit)
		return "unknown flag";
	
	c = tc->flag_map[flag];
	return c->flags[flag - c->flags[0].bit].name;
}


/********************
 * component_name
 ********************/
static inline const char *
component_name(trace_context_t *tc, int flag)
{
	trace_component_t *c;

	if (flag < 0 || flag >= tc->bits.nbit)
		return "unknown flag";
	
	c = tc->flag_map[flag];
	return c->name;
}


/*****************************************************************************
 *                      *** bitmap allocation routines ***                   *
 *****************************************************************************/


/********************
 * ffz
 ********************/
static inline int
ffz(trace_bits_t *bits)
{
	unsigned long *wp, w;
	int            wc, i;
	
	if (bits->nbit <= 32) {
		wp = &bits->u.word;
		wc = 1;
	}
	else {
		wp = bits->u.bits;
		wc = (bits->nbit + 31) / 32;
	}
	
	
	for (i = 0; i < wc; i++) {
		/* find first word with a zero bit */
		if (*wp == 0xffffffffUL) {
			wp++;
			continue;
		}
		
		/* binary search for first zero bit */
		w  = *wp;
		i *= 32;
		
		if ((w & 0xffff) == 0xffff)
			i += 16, w >>= 16;
		if ((w & 0xff) == 0xff)
			i += 8, w >>= 8;
		if ((w & 0xf) == 0xf)
			i += 4, w >>= 4;
		if ((w & 0x3) == 0x3)
			i += 2, w >>= 2;
		if (w & 0x1)
			i  += 1, w >>= 1;

#ifdef __TRACE_CHECK__
		if (w) {
			printf("%s @ %s:%d: BUG: ended up with 0x1 bit\n", __FUNCTION__,
				   __FILE__, __LINE__);
			return -1;
		}
#endif

		return i;
	}
	
	return -1;
}


/********************
 * realloc_bits
 ********************/
static int
realloc_bits(trace_bits_t *tb, int n)
{
	int            wc, diff;
	unsigned long *wp;
	
	if (unlikely(n == tb->nbit))
		return 0;
	
	wc = (n + 31) / 32;

	if (n > tb->nbit) {                               /* grow */
		if ((wp = realloc(tb->u.bits, wc * 4)) == NULL)
			return ENOMEM;
		diff = wc - (tb->nbit + 31) / 32;
		memset(wp + wc - diff, 0, 4 * diff);
		tb->u.bits = wp;
		tb->nbit   = wc * 32;
	}
	else {                                            /* shrink */
		if (wc == 1) {
			wp = tb->u.bits;
			tb->u.word = *wp;
			free(wp);
		}
		else {
			if ((wp = realloc(tb->u.bits, wc * 4)) == NULL)
				return ENOMEM;
			tb->u.bits = wp;
			tb->nbit   = wc * 32;
		}
	}

	return 0;
}


/********************
 * free_bits
 ********************/
static void
free_bits(trace_bits_t *tb)
{
	if (tb->nbit > 32) {
		free(tb->u.bits);
		tb->u.word = 0;
		tb->nbit   = 32;
		
	}
}


/********************
 * alloc_flags
 ********************/
static int
alloc_flags(trace_context_t *tc, trace_component_t *c)
{
	trace_flag_t *fp;
	int           bit, nbit, nflag, missing, last, i;
	

	/*
	 * Notes:
	 *   This piece of code is somewhat naive. It first tries to allocate
	 *   bits starting at the lowest free bit (ie. find lowest bit and hope
	 *   there is enough free consecutive bits). If this fails it enlarges
	 *   the bitmap and occupies the newly allocated bits. It never goes
	 *   on to find other (than the first) large enough free holes in the
	 *   allocation bitmap. This can become a problem if components get
	 *   regularly registered and unregistered which can fragment up the bitmap
	 *   (eg. if the pattern of allocation and freeing is
	 *    a1, a2, f1, a3, f2, a4, f3, a5, f4... and always
	 *    the number of bits for ax is larger than ay, where x < y).
 	 *
	 *   Frequent dynamic registration and unregistration is not expected
	 *   to happen currently so this is not addressed ATM. If necessary,
	 *   code could easily be added to try to find a large enough hole
	 *   before expanding the bitmap...
	 */

	

	/*
	 * try first an optimistic allocation (assume enough bits in first hole)
	 */

	/* realloc if full */
	nbit = 0;
	i    = ffz(&tc->bits);
	if (i < 0) {
		i = tc->bits.nbit;
		if (realloc_bits(&tc->bits, i + 32))
			return ENOMEM;
	}

	/* count and try to allocate in a single pass */
	missing = last = 0;
	bit = i;
	for (nflag = 0, fp = c->flags; fp->name; fp++, nflag++) {
		if (!missing && !tst_bit(&tc->bits, bit)) {
			set_bit(&tc->bits, bit);
			fp->bit = bit;
			if (fp->bitret)
				*fp->bitret = fp->bit;
			last = bit++;
		}
		else {
			fp->bit = -1;
			missing++;
		}
	}
	

	/*
	 * could not allocate for all
	 */
		
	if (missing) {
		if (last == tc->bits.nbit - 1) { /* just ran out of bits */
			if (realloc_bits(&tc->bits, tc->bits.nbit + missing))
				return ENOMEM;
			for (fp = c->flags + nflag - missing; fp->name; fp++) {
#ifdef __TRACE_CHECK__
				if (tst_bit(&tc->bits, bit))
					printf("%s @ %s:%d: BUG: fresh bit %d is not free!!!\n",
						   __FUNCTION__, __FILE__, __LINE__, bit);
#endif
				set_bit(&tc->bits, bit);
				fp->bit = bit++;
				if (fp->bitret)
					*fp->bitret = fp->bit;
			}
		}
		else {                           /* not enough consecutive free bits */
			bit = tc->bits.nbit;         /* brute-force alloc nflag new bits */
			if (realloc_bits(&tc->bits, tc->bits.nbit + nflag))
				return ENOMEM;
			for (fp = c->flags; fp->name; fp++) {
				if (fp->bit >= 0)        /* need to clear old allocation */
					clr_bit(&tc->bits, fp->bit);
				set_bit(&tc->bits, bit);
				fp->bit = bit++;
				if (fp->bitret)
					*fp->bitret = fp->bit;
			}
		}
	}

	return 0;
}


/********************
 * free_flags
 ********************/
static int
free_flags(trace_context_t *tc, trace_component_t *c)
{
	trace_flag_t *fp;
	
	for (fp = c->flags; fp->name; fp++) {
		if (fp->bit >= 0)        /* need to clear old allocation */
			clr_bit(&tc->bits, fp->bit);
		fp->bit = -1;
		if (fp->bitret)
			*fp->bitret = -1;
	}

	return 0;
}


/*****************************************************************************
 *                         *** tag-based filtering ***                       *
 *****************************************************************************/

/********************
 * trace_reset_filters
 ********************/
void
trace_reset_filters(trace_context_t *tc)
{
	list_hook_t           *p, *n;
	trace_simple_filter_t *sf;
	trace_regexp_filter_t *rf;

	CHECK_DEFAULT_CONTEXT(tc);

	list_iter_forw_safe(p, n, &tc->simple_filters) {
		sf = list_entry(p, trace_simple_filter_t, hook);
		list_delete(&sf->hook);
		simple_free(tc, sf);
	}

	list_iter_forw_safe(p, n, &tc->regexp_filters) {
		rf = list_entry(p, trace_regexp_filter_t, hook);
		list_delete(&rf->hook);
		regexp_free(tc, rf);
	}
	
	tc->filter_flags &= ~(FILTER_PASS_EMPTY | FILTER_PASS_ALL);
}


/********************
 * trace_add_simple_filter
 ********************/
int
trace_add_simple_filter(trace_context_t *tc, char *filter_descr)
{
	trace_simple_filter_t *filter;
	trace_tag_t           *tags, *t;
	int                    n, status;

	CHECK_DEFAULT_CONTEXT(tc);

	if (filter_descr == TRACE_NO_TAGS ||
		!strcmp(filter_descr, TRACE_FILTER_NO_TAGS)) {
		tc->filter_flags |= FILTER_PASS_EMPTY;
		return 0;
	}
	if (!strcmp(filter_descr, TRACE_FILTER_ALL_TAGS)) {
		tc->filter_flags |= FILTER_PASS_ALL;
		return 0;
	}
		
	
	if ((tags = trace_parse_filter(filter_descr)) == NULL) {
		status = EINVAL;
		goto failed;
	}
	
	for (n = 0, t = tags; t->key; t++, n++)
		;

	if ((filter = simple_alloc(tc, n, tags)) == NULL) {
		status = EINVAL;
		goto failed;
	}
	
	list_add_tail(&filter->hook, &tc->simple_filters);
	free(tags);
	
	return 0;

 failed:
	if (tags)
		free(tags);
	
	return status;
}


/********************
 * trace_del_simple_filter
 ********************/
int
trace_del_simple_filter(trace_context_t *tc, char *filter_descr)
{
	trace_simple_filter_t *filter, *f;
	trace_tag_t           *tags, *t;
	list_hook_t           *p, *n;
	int                    ntag, status;

	CHECK_DEFAULT_CONTEXT(tc);

	if (filter_descr == TRACE_NO_TAGS ||
		!strcmp(filter_descr, TRACE_FILTER_NO_TAGS)) {
		tc->filter_flags &= ~FILTER_PASS_EMPTY;
		return 0;
	}
	if (!strcmp(filter_descr , TRACE_FILTER_ALL_TAGS)) {
		tc->filter_flags &= ~FILTER_PASS_ALL;
		return 0;
	}
	
	if ((tags = trace_parse_filter(filter_descr)) == NULL) {
		status = EINVAL;
		goto failed;
	}
	
	for (ntag = 0, t = tags; t->key; t++, ntag++)
		;

	if ((filter = simple_alloc(tc, ntag, tags)) == NULL) {
		status = EINVAL;
		goto failed;
	}
	
	free(tags);

	status = ENOENT;
	list_iter_forw_safe(p, n, &tc->simple_filters) {
		f = list_entry(p, trace_simple_filter_t, hook);
		if (simple_identical(filter, f)) {
			list_delete(&f->hook);
			simple_free(tc, filter);
			status = 0;
			/*
			 * we don't check for duplicates yet when we add filters, so
			 * we loop through all the filters here to remove duplicates
			 * break;
			 */
		}
	}
	
	simple_free(tc, filter);

	return status;


 failed:
	if (tags)
		free(tags);
	
	return status;
}


/********************
 * trace_add_regexp_filter
 ********************/
int
trace_add_regexp_filter(trace_context_t *tc, char *filter_descr)
{
	trace_regexp_filter_t *filter;
	trace_tag_t           *tags, *t;
	int                    n, status;

	CHECK_DEFAULT_CONTEXT(tc);

	if (filter_descr == TRACE_NO_TAGS ||
		!strcmp(filter_descr, TRACE_FILTER_NO_TAGS)) {
		tc->filter_flags = FILTER_PASS_EMPTY;
		return 0;
	}
	if (!strcmp(filter_descr , TRACE_FILTER_ALL_TAGS)) {
		tc->filter_flags |= FILTER_PASS_ALL;
		return 0;
	}

	if ((tags = trace_parse_filter(filter_descr)) == NULL) {
		status = EINVAL;
		goto failed;
	}
	
	for (n = 0, t = tags; t->key; t++, n++)
		;
	
	if ((filter = regexp_alloc(tc, n, tags)) == NULL) {
		status = EINVAL;
		goto failed;
	}
	
	list_add_tail(&filter->hook, &tc->regexp_filters);
	free(tags);

#if 0
	{
		list_hook_t           *p;
		trace_regexp_filter_t *f;
		int i = 0;
	
		list_iter_forw(p, &tc->regexp_filters) {
			char fbuf[128];

			f = list_entry(p, trace_regexp_filter_t, hook);
		
			regexp_print(f, fbuf, sizeof(fbuf));
			/*printf("filter #%d: %s\n", i++, fbuf);*/
		}
	}
#endif

	return 0;

	
 failed:
	if (tags)
		free(tags);

	return status;
}


/********************
 * trace_del_regexp_filter
 ********************/
int
trace_del_regexp_filter(trace_context_t *tc, char *filter_descr)
{
	trace_regexp_filter_t *filter, *f;
	trace_tag_t           *tags, *t;
	list_hook_t           *p, *n;
	int                    ntag, status;

	CHECK_DEFAULT_CONTEXT(tc);

	if (filter_descr == TRACE_NO_TAGS ||
		!strcmp(filter_descr, TRACE_FILTER_NO_TAGS)) {
		tc->filter_flags &= ~FILTER_PASS_EMPTY;
		return 0;
	}
	if (!strcmp(filter_descr , TRACE_FILTER_ALL_TAGS)) {
		tc->filter_flags &= ~FILTER_PASS_ALL;
		return 0;
	}


	if ((tags = trace_parse_filter(filter_descr)) == NULL) {
		status = EINVAL;
		goto failed;
	}
	
	for (ntag = 0, t = tags; t->key; t++, ntag++)
		;
	
	if ((filter = regexp_alloc(tc, ntag, tags)) == NULL) {
		status = EINVAL;
		goto failed;
	}

	free(tags);
	
	status = ENOENT;
	list_iter_forw_safe(p, n, &tc->regexp_filters) {
		f = list_entry(p, trace_regexp_filter_t, hook);
		if (regexp_identical(filter, f)) {
			list_delete(&f->hook);
			regexp_free(tc, filter);
			status = 0;
			/*
			 * we don't check for duplicates yet when we add filters, so
			 * we loop through all the filter here to remove duplicates
			 * break;
			 */
		}
	}
	
	regexp_free(tc, filter);

	return status;

 failed:
	if (tags)
		free(tags);

	return status;
}


/********************
 * trace_print_tags
 ********************/
char *
trace_print_tags(trace_tag_t *tags, char *buf, size_t size)
{
#define OVERFLOW_CHECK(n, left) do {			\
		if ((n) > (left))						\
			goto overflow;						\
	} while (0)
	
	char *p, *t;
	int   left, n;

	if (size < 3)
		goto overflow;

	t    = "";
	p    = buf;
	left = size - 1;
	
	*p++ = '{';
	left--;
	
	while (tags->key && left > 0) {
		n = snprintf(p, left, "%s%s=%s", t, tags->key, tags->value);
		OVERFLOW_CHECK(n, left);
		p    += n;
		left -= n;
		t = ",";
		tags++;
	}
	
	n = snprintf(p, left, "}");
	OVERFLOW_CHECK(n, left);
	p    += n;
	left -= n;
	
	*p = '\0';                                 /* safe (left = size - 1)... */
	
 overflow:
	return "<buffer too small>";
}


/********************
 * check_header
 ********************/
static int
check_header(const char *format)
{
	static int warn = 1;

	if (warn) {
		printf("***** %s@%s:%d: implement me, please... *****\n", __FUNCTION__,
			   __FILE__, __LINE__);
		warn = 0;
	}
	
	return format ? 0 : EINVAL; /* XXX TODO: decent format checking */
}


/*****************************************************************************
 *                       *** default context handling ***                    *
 *****************************************************************************/

/********************
 * default_init
 ********************/
static void
default_init(void)
{
	char link_path[32], bin_path[256], *bin_name;
			
	if (__dc.name != NULL)
		return;

	/* determine application name, defaulting to unknown */
	memset(bin_path, 0, sizeof(bin_path));
	sprintf(link_path, "/proc/%d/exe", getpid());
	readlink(link_path, bin_path, sizeof(bin_path) - 1);
	if (bin_path[0]) {
		if ((bin_name = strrchr(bin_path, '/')) == NULL)
			bin_name = bin_path;
		else
			bin_name++;
	}
	else
		bin_name = "unknown";
	
	trace_open(&__dc, bin_name);

	/* make purely flag-based, enable, and set a very simple header */
	trace_add_simple_filter(&__dc, TRACE_FILTER_ALL_TAGS);
	trace_enable(&__dc);
	trace_set_header(&__dc, "[%d %C]");

}


/********************
 * default_context
 ********************/
static inline trace_context_t *
default_context(void)
{
	if (unlikely(__dc.name == NULL))
		default_init();
	
	return &__dc;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
