<?

$test = 1;
$tc   = NULL;

$TRACE_FLAG1 = -1;
$TRACE_FLAG2 = -1;
$TRACE_FLAG3 = -1;


/********************
 * fatal
 ********************/
function fatal($ec, $msg)
{
	printf("fatal error: %s\n", $msg);
	exit($ec);
}


/********************
 * test_init
 ********************/
function test_init()
{
	global $tc;
	global $TRACE_FLAG1, $TRACE_FLAG2, $TRACE_FLAG3;

	$c = array('name' => 'comp1',
			   'flags' => array('flag1' => array('description' => 'blah 1...',
												 'flag' => &$TRACE_FLAG1),
								'flag2' => array('description' => 'blah 2...',
												 'flag' => &$TRACE_FLAG2),
								'flag3' => array('description' => 'blah 3...',
												 'flag' => &$TRACE_FLAG3)));
	
	if (!trace_init() || 
		is_null($tc = trace_open('tracetest')) ||
		!trace_add_component($tc, $c))
		fatal(1, "failed to create a trace context for testing\n");
	
	trace_set_destination($tc, TRACE_TO_STDOUT);
	trace_set_header($tc, "----------\n%d %i: %W\nflag: %f, tags: { %T }\n");
	
	return 0;
}



/********************
 * test_cycle
 ********************/
function test_cycle()
{
	global $tc, $test, $TRACE_FLAG1, $TRACE_FLAG2, $TRACE_FLAG3;



	for ($i = 0; $i < 16; $i++) {
		$itags = array('i'    => "$i",
					   'test' => "$test");
		trace_write($tc, $TRACE_FLAG1, $itags,
					"test #%d, i is %d\n", $test++, $i);
		
		for ($j = 0; $j < 16; $j++) {
			$sum  = $i+$j;
			$prod = $i * $j;

			$jtags = array('i'    => "$i",
						   'j'    => "$j",
						   'sum'  => "$sum",
						   'product' => "$prod");
			trace_write($tc, $TRACE_FLAG2, $jtags,
						"test #%d, i = %d, j = %d\n", $test++, $i, $j);

			trace_write($tc, $TRACE_FLAG3, $jtags, 
						"test #%d, sum = %d, product = %d\n",
						$test++, $i + $j, $i * $j);
		}
	}
}


/********************
 * main test script
 ********************/

$test = 1;
	
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

trace_enable($tc);
/* trace flags are off (by default) so nothing should be printed */
test_cycle();
	
/* no filters installed so nothing should be printed */
trace_on($tc, $TRACE_FLAG1);
trace_on($tc, $TRACE_FLAG3);
test_cycle();

/* flag tests */
/* add a catch-all filter to make suppression effectively flag-based */
trace_add_simple_filter($tc, "all");
	
trace_on($tc, $TRACE_FLAG1);
trace_on($tc, $TRACE_FLAG2);
trace_on($tc, $TRACE_FLAG3);
test_cycle();

trace_on($tc, $TRACE_FLAG1);
trace_off($tc, $TRACE_FLAG2);
trace_off($tc, $TRACE_FLAG3);
test_cycle();

trace_off($tc, $TRACE_FLAG1);
trace_on($tc, $TRACE_FLAG2);
trace_off($tc, $TRACE_FLAG3);
test_cycle();

trace_off($tc, $TRACE_FLAG1);
trace_off($tc, $TRACE_FLAG2);
trace_on($tc, $TRACE_FLAG3);
test_cycle();

trace_off($tc, $TRACE_FLAG1);
trace_on($tc, $TRACE_FLAG2);
trace_on($tc, $TRACE_FLAG3);
test_cycle();

trace_on($tc, $TRACE_FLAG1);
trace_off($tc, $TRACE_FLAG2);
trace_on($tc, $TRACE_FLAG3);
test_cycle();

trace_on($tc, $TRACE_FLAG1);
trace_on($tc, $TRACE_FLAG2);
trace_off($tc, $TRACE_FLAG3);
test_cycle();

/* filter tests */
/* turn on all flags to make suppression effectively filter-based */
trace_on($tc, $TRACE_FLAG1);
trace_on($tc, $TRACE_FLAG2);
trace_on($tc, $TRACE_FLAG3);

/* i == 10 || j == 0 || j == 5 */
trace_reset_filters($tc);
trace_add_simple_filter($tc, "i=10");
trace_add_simple_filter($tc, "j=0");
trace_add_simple_filter($tc, "j=5");
test_cycle();

/* i == 5 || (i == 10 && (j is odd)) */
trace_reset_filters($tc);
trace_add_simple_filter($tc, "i=5");
trace_add_regexp_filter($tc, "i=10 j=^(1|3|5|7|9)$");
test_cycle();


/*
 * clean up and exit
 */

trace_close($tc);
trace_exit();

exit(0);


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
