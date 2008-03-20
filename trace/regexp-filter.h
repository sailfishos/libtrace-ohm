#ifndef TRACE_REGEXP_FILTER_H
#define TRACE_REGEXP_FILTER_H

#include <trace/trace.h>

trace_regexp_filter_t *regexp_alloc(trace_context_t *, int, trace_tag_t *);
void                   regexp_free (trace_context_t *, trace_regexp_filter_t *);
int                    regexp_compare(trace_regexp_filter_t *, trace_tag_t *);
int                    regexp_match_tags(trace_context_t *, trace_tag_t *);


#endif /* TRACE_REGEXP_FILTER_H */
