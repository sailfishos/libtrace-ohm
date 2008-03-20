#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <sys/types.h>

#include <trace/trace.h>

#ifdef __TRACE_REGEXP__
#define REGEXP_TRACE(fmt, args...) do {				   \
		fprintf(stderr, "[regexp]: "fmt, ## args);	   \
	} while (0)
#else
#define REGEXP_TRACE(fmt, args...) do {	} while (0)
#endif

static char *patdup(const char *pattern);

/*****************************************************************************
 *                         *** filter allocation ***                         *
 *****************************************************************************/

/********************
 * regexp_alloc
 ********************/
trace_regexp_filter_t *
regexp_alloc(trace_context_t *tc, int ntag, trace_tag_t *tags)
{
	trace_regexp_filter_t *filter = NULL;
	trace_regexp_tag_t    *tp;
	trace_tag_t           *tag;
	
	if (tags == NULL)
		return NULL;

	if ((filter = malloc(TRACE_REGEXP_FILTER_SIZE(ntag + 1))) == NULL)
		return NULL;
	
	memset(filter, 0, TRACE_REGEXP_FILTER_SIZE(ntag + 1));
	filter->type = TRACE_REGEXP_FILTER;
	list_hook_init(&filter->hook);

	for (tag = tags, tp = &filter->tags[0]; tag->key; tag++, tp++) {
		if ((tp->key   = strdup(tag->key))   == NULL ||
			(tp->value = patdup(tag->value)) == NULL)
			goto failed;
		if (regcomp(&tp->regex, tp->value, REG_EXTENDED | REG_NOSUB) != 0)
			goto failed;
		filter->ntag++;
	}

	return filter;
	
 failed:
	regexp_free(tc, filter);
	return NULL;
}


/********************
 * patdup
 ********************/
static char *
patdup(const char *pattern)
{
	size_t  size = strlen(pattern) + 1;
	char   *buf, *p;

	/*
	 * duplicate the pattern mangling it a bit for us
	 */
	
	if (pattern[0] != '^')
		size += 1;
	
	if ((p = buf = malloc(size)) == NULL)
		return NULL;
	
	if (pattern[0] != '^')
		*p++ = '^';

	strcpy(p, pattern);
	return buf;
}


/********************
 * regexp_free
 ********************/
void
regexp_free(trace_context_t *tc, trace_regexp_filter_t *filter)
{
	trace_regexp_tag_t *tp;

	if (filter == NULL || filter->type != TRACE_REGEXP_FILTER)
		return;
	
	for (tp = &filter->tags[0]; tp->key != NULL; tp++) {
		if (tp->key)
			free((char *)tp->key);
		if (tp->value)
			free((char *)tp->value);
		regfree(&tp->regex);
	}
	
	free(filter);
}


/********************
 * regexp_identical
 ********************/
int
regexp_identical(trace_regexp_filter_t *f1, trace_regexp_filter_t *f2)
{
	trace_regexp_tag_t *t1, *t2;
	int                 match;

	for (t1 = f1->tags; t1->key != NULL; t1++) {
		for (match = 0, t2 = f2->tags; t2->key != NULL && !match; t2++)
			if (!strcmp(t1->key, t2->key) && !strcmp(t1->value, t2->value))
				match = 1;
		if (!match)
			return 0;                              /* not identical */
	}

	return 1;                                      /* are identical */
}


/********************
 * regexp_compare
 ********************/
int
regexp_compare(trace_regexp_filter_t *filter, trace_tag_t *tags)
{
	trace_tag_t        *tt;
	trace_regexp_tag_t *ft;
	int                 match;

	if (filter->type != TRACE_REGEXP_FILTER)
		return EINVAL;

	/*
	 * try to find a matching tag for each filter expression
	 */

	match = 0;
	for (ft = filter->tags; ft->key; ft++) {
		for (tt = tags, match = 0; tt->key && !match; tt++) {
			match = (!strcmp(ft->key, tt->key) &&
					 !regexec(&ft->regex, tt->value, 0, NULL, 0));
		}
		if (!match)                               /* unmatched expression */
			return 1;
	}
	
	return !match;         /* hmm, why... should be equivalent to return 0; */
}


/********************
 * regexp_match_tags
 ********************/
int
regexp_match_tags(trace_context_t *tc, trace_tag_t *tags)
{
	list_hook_t           *p;
	trace_regexp_filter_t *f;
	int i;
	
	/*
	 * This is the slow-path regexp filter checker. It loops through all
	 * the installed filters and checks them against the given tags until
	 * a match is found (or we run out of filters).
	 *
	 * Notes:
	 *     This is _very_ brute force at the moment for much the same reason
	 *     as for simple filters (shortly have not figured out how, yet).
	 *
	 *     One possible way to optimize the filter matching would be to
	 *     combine all the active filters to a single regexp so regexec
	 *     would check them parallel in a single pass. This would require
	 *     combining also the tags to a single string and also the combined
	 *     filter would need to include all the possible permutations (order-
	 *     wise) of the tags. This might make this approach unfeasible.
	 */
	
	if (list_is_empty(&tc->regexp_filters) &&
		list_is_empty(&tc->simple_filters))  /* no filters => don't match */
		return 0;                       /* XXX do we want this semantically ? */

	
	i = 0;
	list_iter_forw(p, &tc->regexp_filters) {
#ifdef __TRACE_REGEXP__
		char fbuf[128], tbuf[128];
#endif

		f = list_entry(p, trace_regexp_filter_t, hook);

#ifdef __TRACE_REGEXP__
		trace_print_tags(tags, tbuf, sizeof(tbuf));
		regexp_print(f, fbuf, sizeof(fbuf));
		REGEXP_TRACE("#%d: %s == %s: ", i++, tbuf, fbuf);
#endif

		if (!regexp_compare(f, tags)) {
			REGEXP_TRACE("YES\n");
			return 1;
		}
		else
			REGEXP_TRACE("NO\n");
	}
	
	return 0;
}


/********************
 * regexp_print
 ********************/
char *
regexp_print(trace_regexp_filter_t *filter, char *buf, size_t size)
{
#define OVERFLOW_CHECK(n, left) do {			\
		if ((n) > (left))						\
			goto overflow;						\
	} while (0)

	trace_regexp_tag_t *tag;
	
	char *p, *t;
	int   left, n;

	if (size < 3)
		goto overflow;

	t    = "";
	p    = buf;
	left = size - 1;
	
	*p++ = '{';
	left--;
	
	tag = filter->tags;
	while (tag->key && left > 0) {
		n = snprintf(p, left, "%s%s=%s", t, tag->key, tag->value);
		OVERFLOW_CHECK(n, left);
		p    += n;
		left -= n;
		t = ",";
		tag++;
	}
	
	n = snprintf(p, left, "}");
	OVERFLOW_CHECK(n, left);
	p    += n;
	left -= n;
	
	*p = '\0';                                 /* safe (left = size - 1)... */
	
 overflow:
	return "<buffer too small>";
}



/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
