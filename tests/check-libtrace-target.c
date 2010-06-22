/******************************************************************************/
/*  This file is part of libtrace                                             */
/*                                                                            */
/*  Copyright (C) 2010 Nokia Corporation.                                     */
/*                                                                            */
/*  This library is free software; you can redistribute                       */
/*  it and/or modify it under the terms of the GNU Lesser General Public      */
/*  License as published by the Free Software Foundation                      */
/*  version 2.1 of the License.                                               */
/*                                                                            */
/*  This library is distributed in the hope that it will be useful,           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          */
/*  Lesser General Public License for more details.                           */
/*                                                                            */
/*  You should have received a copy of the GNU Lesser General Public          */
/*  License along with this library; if not, write to the Free Software       */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  */
/*  USA.                                                                      */
/******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <check.h>


#include <simple-trace/simple-trace.h>
#include "check-libtrace.h"

#define CONTEXT_NAME "context"

static int cid;
static int DBG_FOO, DBG_BAR, DBG_FOOBAR;

TRACE_DECLARE_MODULE(targettest, "module",
                     TRACE_FLAG("foo"   , "flag foo"   , &DBG_FOO),
                     TRACE_FLAG("bar"   , "flag bar"   , &DBG_BAR),
                     TRACE_FLAG("foobar", "flag foobar", &DBG_FOOBAR));

static void
setup(void)
{
    fail_unless(trace_init() == 0);
    fail_unless((cid = trace_context_open(CONTEXT_NAME)) >= 0);
    fail_unless(trace_add_module(cid, &targettest) == 0);
    fail_unless(trace_context_enable(cid) == 0);
    trace_flag_set(DBG_FOO);
    trace_flag_set(DBG_FOOBAR);
}


static void
teardown(void)
{
    fail_unless(trace_del_module(cid, targettest.name) == 0);
    fail_unless(trace_context_close(cid) == 0);
}


START_TEST(test_stdout)
{
    int  fd_out, fd_pipe[2], fd_save;
    char buf[1024];

    fail_unless(trace_context_target(cid, TRACE_TO_STDOUT) == 0);

    fd_out = fileno(stdout);
    fail_unless(capture_fd(fd_out, fd_pipe, &fd_save) == 0);

    fail_unless(trace_printf(DBG_FOO, "foo") > 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) > 0);
    fail_unless(trace_printf(DBG_BAR, "bar") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);
    fail_unless(trace_printf(DBG_FOOBAR, "foobar") > 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) > 0);

    fail_unless(release_fd(fd_out, fd_pipe, fd_save) == 0);
}
END_TEST


START_TEST(test_file)
{
#define TEST_FILE "/tmp/trace-test.log"
    int  fd_out;
    char buf[1024];

    fail_unless(trace_context_enable(cid) == 0);

    fail_unless(trace_flag_set(DBG_FOO) == 0);
    fail_unless(trace_flag_set(DBG_FOOBAR) == 0);

    fail_unless(trace_context_target(cid, TRACE_TO_FILE(TEST_FILE)) == 0);
    
    fd_out = open(TEST_FILE, O_RDONLY);

    fail_unless(trace_printf(DBG_FOO, "foo") > 0);
    fail_unless(trace_printf(DBG_BAR, "bar") == 0);
    fail_unless(trace_printf(DBG_FOOBAR, "foobar") > 0);
    fail_unless(read(fd_out, buf, sizeof(buf)) > 0);
    
    close(fd_out);
    unlink(TEST_FILE);
}
END_TEST


void
chktrace_target_tests(Suite *suite)
{
    TCase *tc;

    tc = tcase_create("trace target");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_stdout);
    tcase_add_test(tc, test_file);
    suite_add_tcase(suite, tc);
}



/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */


