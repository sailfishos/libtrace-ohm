#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <check.h>


#include <simple-trace/simple-trace.h>
#include "check-libtrace.h"

#define CONTEXT_NAME "test"

static int cid;
static int DBG_FOO, DBG_BAR, DBG_FOOBAR;

TRACE_DECLARE_MODULE(flagtest, "test",
                     TRACE_FLAG("foo"   , "flag foo"   , &DBG_FOO),
                     TRACE_FLAG("bar"   , "flag bar"   , &DBG_BAR),
                     TRACE_FLAG("foobar", "flag foobar", &DBG_FOOBAR));


static void
setup(void)
{
    fail_unless(trace_init() == 0);
    fail_unless((cid = trace_context_open(CONTEXT_NAME)) >= 0);
    fail_unless(trace_add_module(cid, &flagtest) == 0);
}


static void
teardown(void)
{
    fail_unless(trace_del_module(cid, flagtest.name) == 0);
    fail_unless(trace_context_close(cid) == 0);
}


START_TEST(disabled_context)
{
    int  fd_err, fd_pipe[2], fd_save;
    char buf[1024];

    fd_err = fileno(stderr);
    fail_unless(capture_fd(fd_err, fd_pipe, &fd_save) == 0);

    fail_unless(trace_printf(DBG_FOO, "foo") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);
    fail_unless(trace_printf(DBG_BAR, "bar") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);
    fail_unless(trace_printf(DBG_FOOBAR, "foobar") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);

    fail_unless(release_fd(fd_err, fd_pipe, fd_save) == 0);
}
END_TEST


START_TEST(disabled_flag)
{
    int  fd_err, fd_pipe[2], fd_save;
    char buf[1024];

    fail_unless(trace_context_enable(cid) == 0);

    fd_err = fileno(stderr);
    fail_unless(capture_fd(fd_err, fd_pipe, &fd_save) == 0);

    fail_unless(trace_printf(DBG_FOO, "foo") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);
    fail_unless(trace_printf(DBG_BAR, "bar") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);
    fail_unless(trace_printf(DBG_FOOBAR, "foobar") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);

    fail_unless(release_fd(fd_err, fd_pipe, fd_save) == 0);
}
END_TEST


START_TEST(enabled_flag)
{
    int  fd_err, fd_pipe[2], fd_save;
    char buf[1024];

    fail_unless(trace_context_enable(cid) == 0);

    trace_configure("test=+all");

    fail_unless(trace_flag_set(DBG_FOO) == 0);
    fail_unless(trace_flag_set(DBG_FOOBAR) == 0);

    fd_err = fileno(stderr);
    fail_unless(capture_fd(fd_err, fd_pipe, &fd_save) == 0);

    fail_unless(trace_printf(DBG_FOO, "foo") > 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) > 0);
    fail_unless(trace_printf(DBG_BAR, "bar") == 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) < 0 && errno == EAGAIN);
    fail_unless(trace_printf(DBG_FOOBAR, "foobar") > 0);
    fail_unless(read(fd_pipe[0], buf, sizeof(buf)) > 0);

    fail_unless(release_fd(fd_err, fd_pipe, fd_save) == 0);
}
END_TEST


void
chktrace_flag_tests(Suite *suite)
{
    TCase *tc;

    tc = tcase_create("trace flags");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, disabled_context);
    tcase_add_test(tc, disabled_flag);
    tcase_add_test(tc, enabled_flag);
    suite_add_tcase(suite, tc);
}



/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */


