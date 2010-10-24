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
#include <check.h>

#include <simple-trace/simple-trace.h>
#include "check-libtrace.h"

#define CONTEXT_NAME "test"


START_TEST(missing_open)
{
    /* trace flags */
    int DBG_GRAPH, DBG_VAR, DBG_RESOLVE, DBG_ACTION, DBG_VM;

    TRACE_DECLARE_MODULE(test, "test",
        TRACE_FLAG("graph"  , "dependency graph"    , &DBG_GRAPH),
        TRACE_FLAG("var"    , "variable handling"   , &DBG_VAR),
        TRACE_FLAG("resolve", "dependency resolving", &DBG_RESOLVE),
        TRACE_FLAG("action" , "action processing"   , &DBG_ACTION),
        TRACE_FLAG("vm"     , "VM execution"        , &DBG_VM));

    fail_unless(trace_add_module(5, &test) < 0);
}
END_TEST


START_TEST(multiple_open)
{
    int cid;

    fail_unless((cid = trace_context_open(CONTEXT_NAME)) >= 0);
    fail_unless(trace_context_open(CONTEXT_NAME) == cid);
}
END_TEST


START_TEST(spurious_close)
{
    fail_unless(trace_context_close(3) >= 0);
}
END_TEST


START_TEST(normal_open_close)
{
    int cid;

    fail_unless((cid = trace_context_open(CONTEXT_NAME)) >= 0);
    fail_unless(trace_context_close(cid) == 0);
}
END_TEST


START_TEST(multiple_close)
{
    int cid;

    fail_unless((cid = trace_context_open(CONTEXT_NAME)) >= 0);
    fail_unless(trace_context_close(cid) == 0);
    fail_unless(trace_context_close(cid) < 0);
}
END_TEST


void
chktrace_context_tests(Suite *suite)
{
    TCase *tc;

    tc = tcase_create("trace contexts");
    tcase_add_test(tc, missing_open);
    tcase_add_test(tc, multiple_open);
    tcase_add_test(tc, spurious_close);
    tcase_add_test(tc, normal_open_close);
    tcase_add_test(tc, multiple_close);
    suite_add_tcase(suite, tc);
}



/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */


