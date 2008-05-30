<?php

$arr = array();
$arr[] = 42;
$arr[] = 43;
$arr[] = 44;

var_dump($arr);

$flag1 = -1;
$flag2 = -1;
$flag3 = -1;

$flags = array();
$flags['test1'] = array('description' => 'test flag 1', 'flag' => &$flag1);
$flags['test2'] = array('description' => 'test flag 2', 'flag' => &$flag2);
$flags['test3'] = array('description' => 'test flag 3', 'flag' => &$flag3);

var_dump($flags);


$c = array('name' => 'nsntest',
	   'flags' => array('test1' => array('description' => 'test flag 1',
					     'flag' => &$flag1),
			    'test2' => array('description' => 'test flag 2',
					     'flag' => &$flag2),
			    'test3' => array('description' => 'test flag 3',
					     'flag' => &$flag3)));

$foo = array('test1' => array('description' => 'test flag 1',
			      'flag' => &$flag1),
	     'test2' => array('description' => 'test flag 2',
			      'flag' => &$flag2),
	     'test3' => array('description' => 'test flag 3',
			      'flag' => &$flag3));

$foo['test1']['flag'] = 3;

print_r($foo);
print_r($c);
print_r($flags);

?>