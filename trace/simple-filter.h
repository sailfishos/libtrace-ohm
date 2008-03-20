#ifndef TRACE_SIMPLE_FILTER_H_INCLUDED
#define TRACE_SIMPLE_FILTER_H_INCLUDED

#include <trace/trace.h>


trace_filter_t *simple_alloc(trace_context_t *tc, int ntag,
			     trace_tag_t *tags);
void simple_free(trace_context_t *tc, trace_filter_t *f);
int  simple_compare(trace_filter_t *filter, trace_tag_t *tags);
int  simple_match_tags(trace_context_t *tc, trace_tag_t *tags);




#endif /* TRACE_SIMPLE_FILTER_H_INCLUDED */
