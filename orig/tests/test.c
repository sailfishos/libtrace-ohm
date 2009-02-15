#include <stdio.h>
#include <stdlib.h>

#include <trace/trace.h>

#define fatal(ec, fmt, args...) do {				 \
		printf("fatal error: "fmt"\n", ## args);     \
		exit(ec);								     \
	} while (0)


#undef TRACE_DECLARE_FLAGS
#undef TRACE_FLAGS_INIT
#undef TRACE_DECLARE_COMPONENT
#undef TRACE_COMPONENT_INIT


#define TRACE_DECLARE_COMPONENT(c, n, ...)		\
	trace_flag_t __trace_flags_##c[] = {		\
		__VA_ARGS__,							\
		TRACE_FLAG_LAST							\
	};											\
	trace_component_t c = {						\
		.hook  = { &c.hook, &c.hook },			\
		.name  = (n),							\
		.flags = __trace_flags_##c				\
	}


int TRACE_FLAG1, TRACE_FLAG2, TRACE_FLAG3, TRACE_FLAG4, TRACE_FLAG5;

TRACE_DECLARE_COMPONENT(test, "test",
						TRACE_FLAG_INIT("flag1", "flag 1", &TRACE_FLAG1),
						TRACE_FLAG_INIT("flag2", "flag 2", &TRACE_FLAG2),
						TRACE_FLAG_INIT("flag3", "flag 3", &TRACE_FLAG3)  );


int
main(int argc, char *argv[])
{
	trace_context_t tc;
	char            buf[1024];

	TRACE_DECLARE_COMPONENT(foo, "foo",
							TRACE_FLAG_INIT("flag1", "flag 1", &TRACE_FLAG4),
							TRACE_FLAG_INIT("flag2", "flag 2", &TRACE_FLAG5));

	int TRACE_BAR1, TRACE_BAR2;
	
	TRACE_FLAGSET(bar,
				  TRACE_FLAG("bar1", "bar 1", &TRACE_BAR1),
				  TRACE_FLAG("bar2", "bar 2", &TRACE_BAR2));
	
	printf("address of test: %p\n", &test);

	printf("address of bar: %p\n", &bar);

	if (trace_init() != 0 || 
		trace_open(&tc, "tracetest") != 0 ||
		trace_add_component(&tc, &test) ||
		trace_add_component(&tc, &foo))
		fatal(1, "failed to initialize trace context for testing");

	if (trace_add_flagset(&bar)

	if (trace_list_flags(&tc, buf, sizeof(buf), NULL, NULL) < 0)
		fatal(2, "failed to list trace flags");
	
	printf("list of trace flags:\n%s\n", buf);
	
	return 0;
}




/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 * vim:set expandtab shiftwidth=4:
 */


