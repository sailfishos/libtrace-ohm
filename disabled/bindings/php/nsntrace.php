<?php

define('MODULE', 'nsntrace');

if (!extension_loaded(MODULE)) {
	dl(MODULE . '.' . PHP_SHLIB_SUFFIX);
	if (!extension_loaded(MODULE)) {
		prinf("failed to load the extension %s\n", MODULE);
		exit(1);
	}
}

$functions = get_extension_funcs(MODULE);
echo "Functions available in the test extension:\n";
foreach($functions as $func) {
    echo $func."\n";
}
echo "\n";

/*
 * test the trace extenstion
 */

if (($err = trace_init()) != 0) {
	printf("failed to initialize trace extension\n");
	exit(2);
}

printf("trace library initialized...\n");

if (is_null($tc = trace_open('test'))) {
	printf("failed to create a trace context\n");
	exit(3);
}

printf("trace context \"test\" created...\n");

$c1 = array('name' => 'nsntest1',
		   'flags' => array('test1' => array('description' => 'test flag 1',
											 'flag' => &$c1flag1),
							'test2' => array('description' => 'test flag 2',
											 'flag' => &$c1flag2),
							'test3' => array('description' => 'test flag 3',
											 'flag' => &$c1flag3)));

$c2 = array('name' => 'nsntest2',
		   'flags' => array('test1' => array('description' => 'test flag 1',
											 'flag' => &$c2flag1),
							'test2' => array('description' => 'test flag 2',
											 'flag' => &$c2flag2),
							'test3' => array('description' => 'test flag 3',
											 'flag' => &$c2flag3)));

if (($err = trace_add_component($tc, $c1)) != 0) {
	printf("failed to add trace component \"%s\"\n", $c1['name']);
	exit(4);
}

printf("component \"%s\" registered...\n", $c1['name']);

print_r($c1);

if (($err = trace_add_component($tc, $c2)) != 0) {
	printf("failed to add trace component \"%s\"\n", $c2['name']);
	exit(4);
}

printf("component \"%s\" registered...\n", $c2['name']);

print_r($c2);

trace_enable($tc);
if (trace_on($tc, $c1flag1) != 0) {
	printf("failed to enable trace flag %d\n", $c1flag1);
}

$tags = array('from' => 'sip:a_user@foo.com',
			  'to' => 'sip:callee@bar.org',
			  'foo' => 'bar',
			  'foobar' => 'barfoo');

$i = 1;
trace_write($tc, $c1flag1, $tags, "trace message #%d\n", $i++);
trace_write($tc, $c1flag1, NULL, "trace message #%d\n", $i++);

if (($err = trace_del_component($tc, $c1['name'])) != 0) {
	printf("failed to delete component \"%s\"\n", $c1['name']);
	exit(5);
}

printf("component \"%s\" unregistered...\n", $c1['name']);

if (($err = trace_del_component($tc, $c2['name'])) != 0) {
	printf("failed to delete component \"%s\"\n", $c2['name']);
	exit(5);
}

printf("component \"%s\" unregistered...\n", $c2['name']);


trace_close($tc);
trace_exit();




/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */


?>
