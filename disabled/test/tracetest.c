/*************************************************************************
This file is part of libtrace

Copyright (C) 2010 Nokia Corporation.

This library is free software; you can redistribute
it and/or modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation
version 2.1 of the License.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
USA.
*************************************************************************/


#include <stdio.h>
#include <stdlib.h>

#include <trace/trace.h>

#define fatal(ec, fmt, args...) do {				 \
		printf("fatal error: "fmt"\n", ## args); \
		exit(ec);								 \
	} while (0)


int             test;
trace_context_t tc;

int TRACE_FLAG1, TRACE_FLAG2, TRACE_FLAG3;

TRACE_DECLARE_FLAGS(flags,
					TRACE_FLAG_INIT("flag1", "blah 1...", &TRACE_FLAG1),
					TRACE_FLAG_INIT("flag2", "blah 2...", &TRACE_FLAG2),
					TRACE_FLAG_INIT("flag3", "blah 3...", &TRACE_FLAG3));

TRACE_DECLARE_COMPONENT(comp1, "comp1", flags);


/********************
 * test_init
 ********************/
int
test_init(void)
{
	if (trace_init() != 0 || 
		trace_open(&tc, "tracetest") != 0 ||
		trace_add_component(&tc, &comp1))
		fatal(1, "failed to initialize trace context for testing");

	trace_set_destination(&tc, TRACE_TO_STDOUT);
	trace_set_header(&tc, "----------\n%d %i: %W\nflag: %f, tags: { %T }\n");
	
	return 0;
}



/********************
 * test_cycle
 ********************/
void
test_cycle(void)
{
	int i, j;
	
	for (i = 0; i < 16; i++) {
		TRACE_DECLARE_TAGS(itags, 2, 128);

		trace_tag_addf(&itags, "i", "%d", i);
		trace_tag_addf(&itags, "test", "%d", test);
		trace_write(&tc, TRACE_FLAG1, &itags,
					"test #%d, i is %d\n", test, i); test++;
		
		
		for (j = 0; j < 16; j++) {
			TRACE_DECLARE_TAGS(jtags, 8, 256);
			trace_tag_addf(&jtags, "i", "%d", i);
			trace_tag_addf(&jtags, "j", "%d", j);
			trace_tag_addf(&jtags, "sum", "%d", i+j);
			trace_tag_addf(&jtags, "product", "%d", i*j);
			
			trace_write(&tc, TRACE_FLAG2, &jtags,
						"test #%d, i = %d, j = %d\n", test, i, j); test++;

			trace_write(&tc, TRACE_FLAG3, &jtags, 
						"test #%d, sum = %d, product = %d\n", test, i+j, i*j);
			test++;
		}
	}
}


int
main(int argc, char *argv[])
{
	test = 1;
	
	/*
	 * initialize tracing
	 */

	test_init();


	/*
	 * run a couple of different test cycles
	 */
	
	/* test built-int default settings */
	/* tracing is disabled (by default) so nothing should be printed */
	test_cycle();

	trace_enable(&tc);
	/* trace flags are off (by default) so nothing should be printed */
	test_cycle();
	
	/* no filters installed so nothing should be printed */
	trace_on(&tc, TRACE_FLAG1);
	trace_on(&tc, TRACE_FLAG3);
	test_cycle();
	
	/* flag tests */
	/* add a catch-all filter to make suppression effectively flag-based */
	trace_add_simple_filter(&tc, TRACE_FILTER_ALL_TAGS);
	
	trace_on(&tc, TRACE_FLAG1);
	trace_on(&tc, TRACE_FLAG2);
	trace_on(&tc, TRACE_FLAG3);
	test_cycle();

	trace_on(&tc, TRACE_FLAG1);
	trace_off(&tc, TRACE_FLAG2);
	trace_off(&tc, TRACE_FLAG3);
	test_cycle();

	trace_off(&tc, TRACE_FLAG1);
	trace_on(&tc, TRACE_FLAG2);
	trace_off(&tc, TRACE_FLAG3);
	test_cycle();

	trace_off(&tc, TRACE_FLAG1);
	trace_off(&tc, TRACE_FLAG2);
	trace_on(&tc, TRACE_FLAG3);
	test_cycle();

	trace_off(&tc, TRACE_FLAG1);
	trace_on(&tc, TRACE_FLAG2);
	trace_on(&tc, TRACE_FLAG3);
	test_cycle();

	trace_on(&tc, TRACE_FLAG1);
	trace_off(&tc, TRACE_FLAG2);
	trace_on(&tc, TRACE_FLAG3);
	test_cycle();

	trace_on(&tc, TRACE_FLAG1);
	trace_on(&tc, TRACE_FLAG2);
	trace_off(&tc, TRACE_FLAG3);
	test_cycle();
	
	/* filter tests */
	/* turn on all flags to make suppression effectively filter-based */
	trace_on(&tc, TRACE_FLAG1);
	trace_on(&tc, TRACE_FLAG2);
	trace_on(&tc, TRACE_FLAG3);

	/* i == 10 || j == 0 || j == 5 */
	trace_reset_filters(&tc);
	trace_add_simple_filter(&tc, "i=10");
	trace_add_simple_filter(&tc, "j=0");
	trace_add_simple_filter(&tc, "j=5");
	test_cycle();

	/* i == 5 || (i == 10 && (j is odd)) */
	trace_reset_filters(&tc);
	trace_add_simple_filter(&tc, "i=5");
	trace_add_regexp_filter(&tc, "i=10 j=^(1|3|5|7|9)$");
	test_cycle();
	

	/*
	 * clean up and exit
	 */

	trace_close(&tc);
	trace_exit();

	return 0;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
