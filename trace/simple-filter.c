#include <stdlib.h>
#include <string.h>

#include <trace/trace.h>

#if 0
#define SIMPLE_TRACE(fmt, args...) do {				   \
		fprintf(stderr, "[simple]: "fmt"\n", ## args); \
	} while (0)
else
#define SIMPLE_TRACE(fmt, args...) do {	} while (0)
#endif



/*****************************************************************************
 *                          *** filter allocation ***                        *
 *****************************************************************************/

/********************
 * simple_alloc
 ********************/
trace_simple_filter_t *
simple_alloc(trace_context_t *tc, int ntag, trace_tag_t *tags)
{
	trace_simple_filter_t *filter = NULL;
	trace_simple_tag_t    *tp;
	trace_tag_t           *tag;
	size_t                 key_len, val_len;

	if (tags == NULL)
		return NULL;

	if ((filter = malloc(TRACE_SIMPLE_FILTER_SIZE(ntag + 1))) == NULL)
		return NULL;
	
	memset(filter, 0, TRACE_SIMPLE_FILTER_SIZE(ntag + 1));
	filter->type = TRACE_SIMPLE_FILTER;
	list_hook_init(&filter->hook);

	/*
	 * Notes: We could calculate hash while copying... But then we'd
	 *        have two implementations as hash_tags is needed
	 *        for calculating the real tag hashes anyway...
	 */
	for (tag = tags, tp = &filter->tags[0]; tags->key; tags++, tp++) {
		key_len = strlen(tag->key);
		val_len = strlen(tag->value);
		if ((tp->key = malloc(key_len + val_len + 2)) == NULL)
			goto failed;
		tp->value = tp->key + key_len + 1;
		strcpy((char *)tp->key, tags->key);
		strcpy((char *)tp->value, tags->value);
	}

	/* XXX YUCK !! We really need to redo the hashing. Now we blindly assume
	 * and trust trace_simple_tag_t is identical to trace_tag_t. */
#warning "Argh.... NO, NO, NO, NO, NOOOoooooooo......"
	filter->hash = trace_hash_tags((trace_tag_t *)filter->tags);
	
	return filter;


 failed:
	simple_free(tc, filter);
	return NULL;
}


/********************
 * simple_free
 ********************/
void
simple_free(trace_context_t *tc, trace_simple_filter_t *filter)
{
	trace_simple_tag_t *tp;
	
	if (filter == NULL || filter->type != TRACE_SIMPLE_FILTER)
		return;
	
	for (tp = filter->tags; tp->key; tp++)
		free((char *)tp->key);                      /* frees both key & value */
	
	free(filter);
}


/********************
 * simple_identical
 ********************/
int
simple_identical(trace_simple_filter_t *f1, trace_simple_filter_t *f2)
{
	trace_simple_tag_t *t1, *t2;
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
 * simple_compare
 ********************/
int
simple_compare(trace_simple_filter_t *filter, trace_tag_t *tags)
{
	trace_tag_t        *tt;
	trace_simple_tag_t *ft;
	int                 match;
	
	if (filter->type != TRACE_SIMPLE_FILTER)
		return EINVAL;

	/*
	 * try to find a matching tag for each filter expression
	 */
	
	match = 0;
	for (ft = filter->tags; ft->key; ft++) {
		for (tt = tags, match = 0; tt->key && !match; tt++) {
			match = (ft->hash == tt->hash &&
					 !strcmp(ft->key, tt->key) &&
					 !strcmp(ft->value, tt->value));
		}
		if (!match)                               /* unmatched expression */
			return 1;
	}

	return !match;          /* hmm, why... should be equivalent to return 0; */
}


/********************
 * simple_match_tags
 ********************/
int
simple_match_tags(trace_context_t *tc, trace_tag_t *tags)
{
	list_hook_t           *p;
	trace_simple_filter_t *f;
	
	
	/*
	 * This is the slow-path simple filter checker. It loops through
	 * all the filters and checks them against the given tags until a
	 * match is found (or we run out of filters).
	 *
	 * Notes:
	 *     This is _very_ brute force at the moment since we have not
	 *     worked out a reasonable strategy to use the hashes to speed
	 *     this up. First we need to find a reasonable way to calculate
	 *     a tagset level hash from the individual tag hashes. Then we
	 *     need to find a way to check only the relevant filters. The
	 *     basic problem is that a tagset is supposed to be matched by
	 *     itself and all of its subsets. It is not trivial to come
	 *     up with hash functions that capture this (if possible at all)
	 *     especially considering the fact that a tagset is an unordered
	 *     set of tags.
	 *
	 *     One potential hash functions is simple multiplication. It has
	 *     the desirable property of commutativity (term orderindependence).
	 *     If we make (= map) every hash to a prime then the has of a
	 *     set of tags is the product of the inidividual hashes. A filter then
	 *     matches a set of tags if the tagset hash is divisible with the
	 *     filter hash. On the downside we need to divide which is a slow
	 *     operation. Similarly we need to take care of overflows.
	 */


	if (list_is_empty(&tc->simple_filters) &&
		list_is_empty(&tc->regexp_filters))  /* no filters => don't match */
		return 0;                       /* XXX do we want this semantically ? */
	
	list_iter_forw(p, &tc->simple_filters) {
		f = list_entry(p, trace_simple_filter_t, hook);
		
		if (!simple_compare(f, tags))
			return 1;
	}
	
	return 0;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
