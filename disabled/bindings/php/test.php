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