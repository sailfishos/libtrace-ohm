##############################################################################
#  This file is part of libtrace                                             #
#                                                                            #
#  Copyright (C) 2010 Nokia Corporation.                                     #
#                                                                            #
#  This library is free software; you can redistribute                       #
#  it and/or modify it under the terms of the GNU Lesser General Public      #
#  License as published by the Free Software Foundation                      #
#  version 2.1 of the License.                                               #
#                                                                            #
#  This library is distributed in the hope that it will be useful,           #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of            #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          #
#  Lesser General Public License for more details.                           #
#                                                                            #
#  You should have received a copy of the GNU Lesser General Public          #
#  License along with this library; if not, write to the Free Software       #
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  #
#  USA.                                                                      #
##############################################################################

#!/usr/bin/python

import sys
from _nsntrace import *

test = 1
tc   = None

TRACE_FLAG1 = -1
TRACE_FLAG2 = -1
TRACE_FLAG3 = -1


####################
# fatal
####################
def fatal(ec, msg):
	print 'fatal error: ' + msg
	sys.exit(ec)


####################
# test_init
####################
def test_init():
	global tc
	global TRACE_FLAG1, TRACE_FLAG2, TRACE_FLAG3
	
	c = { 'name': 'comp1',
	      'flags': { 'flag1': { 'description': 'blah 1...', 'flag': -1 },
			 'flag2': { 'description': 'blah 2...', 'flag': -1 },
			 'flag3': { 'description': 'blah 3...', 'flag': -1 }}}

	try:
		trace_init()
	except:
		fatal(1, "failed to create trace context for testing\n");

	try:
		tc = trace_open('tracetest')
	except:
		fatal(1, "failed to create trace context for testing\n")
    
	trace_add_component(tc, c)
    
	TRACE_FLAG1 = c['flags']['flag1']['flag'];
	TRACE_FLAG2 = c['flags']['flag2']['flag'];
	TRACE_FLAG3 = c['flags']['flag3']['flag'];
	
	trace_set_destination(tc, TRACE_TO_STDOUT);
	trace_set_header(tc, "----------\n%d %i: %W\nflag: %f, tags: { %T }\n");


####################
# test_cycle
####################
def test_cycle():
	global tc, test, TRACE_FLAG1, TRACE_FLAG2, TRACE_FLAG3
    
	for i in range(0, 16):
		itags = { 'i': ("%d" % i), 'test': ("%d" % test) }
		trace_write(tc, TRACE_FLAG1, itags,	"test #%d, i is %d\n", test, i)
		test += 1
		
		for j in range(0, 16):
			sum  = i + j;
			prod = i * j;
            
			jtags = { 'i': ("%d" % i), 'j': ("%d" % j),
				  'sum': ("%d" % sum), 'product': ("%d" % prod) }
			trace_write(tc, TRACE_FLAG2, jtags,
				    "test #%d, i = %d, j = %d\n", test, i, j)
			test += 1
		
			trace_write(tc, TRACE_FLAG3, jtags, 
				    "test #%d, sum = %d, product = %d\n",
				    test, sum, prod)
			test += 1


####################
# main test script
####################

	
#
# initialize tracing
#

test_init()
test = 1


#
# run a couple of different test cycles
#

# test built-int default settings
# tracing is disabled (by default) so nothing should be printed 
test_cycle()

trace_enable(tc)
# trace flags are off (by default) so nothing should be printed
test_cycle()
	
# no filters installed so nothing should be printed
trace_on(tc, TRACE_FLAG1)
trace_on(tc, TRACE_FLAG3)
test_cycle()

# flag tests
# add a catch-all filter to make suppression effectively flag-based
trace_add_simple_filter(tc, "all")

trace_on(tc, TRACE_FLAG1)
trace_on(tc, TRACE_FLAG2)
trace_on(tc, TRACE_FLAG3)
test_cycle()

trace_on(tc, TRACE_FLAG1)
trace_off(tc, TRACE_FLAG2)
trace_off(tc, TRACE_FLAG3)
test_cycle()

trace_off(tc, TRACE_FLAG1)
trace_on(tc, TRACE_FLAG2)
trace_off(tc, TRACE_FLAG3)
test_cycle()

trace_off(tc, TRACE_FLAG1)
trace_off(tc, TRACE_FLAG2)
trace_on(tc, TRACE_FLAG3)
test_cycle()

trace_off(tc, TRACE_FLAG1)
trace_on(tc, TRACE_FLAG2)
trace_on(tc, TRACE_FLAG3)
test_cycle()

trace_on(tc, TRACE_FLAG1)
trace_off(tc, TRACE_FLAG2)
trace_on(tc, TRACE_FLAG3)
test_cycle()

trace_on(tc, TRACE_FLAG1)
trace_on(tc, TRACE_FLAG2)
trace_off(tc, TRACE_FLAG3)
test_cycle()

# filter tests
# turn on all flags to make suppression effectively filter-based
trace_on(tc, TRACE_FLAG1)
trace_on(tc, TRACE_FLAG2)
trace_on(tc, TRACE_FLAG3)

# i == 10 || j == 0 || j == 5
trace_reset_filters(tc)
trace_add_simple_filter(tc, "i=10")
trace_add_simple_filter(tc, "j=0")
trace_add_simple_filter(tc, "j=5")
test_cycle()

# i == 5 || (i == 10 && (j is odd))
trace_reset_filters(tc)
trace_add_simple_filter(tc, "i=5")
trace_add_regexp_filter(tc, "i=10 j=^(1|3|5|7|9)$")
test_cycle()


#
# clean up and exit
#

trace_close(tc)
trace_exit()

sys.exit(0)



#
# Local variables:
#  c-indent-level: 4
#  c-basic-offset: 4
#  tab-width: 4
# End:
#
