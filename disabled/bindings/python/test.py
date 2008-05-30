#!/usr/bin/python

from _nsntrace import *

def foo():
    print 'foo'

_nsntrace.foo = foo

def trace_test():
    trace_init()

    tc = trace_open('test')

    c1 = { 'name': 'nsntest1',
           'flags': { 'test1': { 'description': 'test flag 1', 'flag': -1 },
                      'test2': { 'description': 'test flag 2', 'flag': -1 },
                      'test3': { 'description': 'test flag 3', 'flag': -1 }}}

    trace_add_component(tc, c1)

    trace_enable(tc)

    flag1 = c1['flags']['test1']['flag']
    flag2 = c1['flags']['test2']['flag']
    flag3 = c1['flags']['test3']['flag']
    trace_on(tc, flag1)

    print 'flag1 is ', flag1
    print 'flag2 is ', flag2
    print 'flag3 is ', flag3
    
    #trace_set_header(tc, '====\n%D %i {%W}\n%T\n----');
    #trace_set_destination(tc, TRACE_TO_STDOUT)

    tags = { 'from': 'sip:a_user@foo.com',
             'to': 'sip:callee@bar.org',
             'foo': 'bar',
             'foobar': 'barfoo' }

    trace_write(tc, flag1, tags, "trace message %d\n", 1)
    
    #trace_del_component(c1)
    
    trace_close(tc)
    
    trace_exit()

trace_test()
foo()
foo()
foo()
