#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <check.h>


#include <simple-trace/simple-trace.h>
#include "check-libtrace.h"

#define TEST_CONTEXT TRACE_DEFAULT_NAME
#define TEST_MODULE  "test-module"
#define TEST_FLAG    "test-flag"
#define TEST_MESSAGE "The quick brown fox jumps over the lazy dog."

static int cid;
static int DBG_TEST;

TRACE_DECLARE_MODULE(defaulttest, TEST_MODULE,
                     TRACE_FLAG(TEST_FLAG, "flag foo", &DBG_TEST));

static int   fd_out, fd_pipe[2], fd_save;
static FILE *stdtrc;


static void
setup(void)
{
    fail_unless(trace_init() == 0);
    cid = TRACE_DEFAULT_CONTEXT;
    fail_unless(trace_add_module(cid, &defaulttest) == 0);

    fail_unless(trace_context_enable(cid) == 0);
    trace_flag_set(DBG_TEST);

    fail_unless(trace_context_target(cid, TRACE_TO_STDERR) == 0);

    fd_out = fileno(stderr);
    fail_unless(capture_fd(fd_out, fd_pipe, &fd_save) == 0);
    fail_unless((stdtrc = fdopen(fd_pipe[0], "r")) != NULL);


}


static void
teardown(void)
{
    fail_unless(release_fd(fd_out, fd_pipe, fd_save) == 0);
    fail_unless(trace_del_module(cid, defaulttest.name) == 0);
    fail_unless(trace_context_close(cid) == 0);
}


START_TEST(context_name)
{
    char ctx[256], msg[256];
    
    fail_unless(trace_context_format(cid, "[%c] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0);
    fail_unless(fscanf(stdtrc, "[%[a-zA-Z-]] %[a-zA-Z. ]", ctx, msg) > 0);
    fail_unless(!strcmp(ctx, TEST_CONTEXT));
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(module_name)
{
    char mod[256], msg[256];
    
    fail_unless(trace_context_format(cid, "[%m] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0);
    fail_unless(fscanf(stdtrc, "[%[a-zA-Z-]] %[a-zA-Z. ]", mod, msg) > 0);
    fail_unless(!strcmp(mod, TEST_MODULE));
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(flag_name)
{
    char flag[256], msg[256];
    
    fail_unless(trace_context_format(cid, "[%f] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0);
    fail_unless(fscanf(stdtrc, "[%[a-zA-Z-]] %[a-zA-Z. ]", flag, msg) > 0);
    fail_unless(!strcmp(flag, TEST_FLAG));
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(where)
{
    const char *file, *function;
    char        fi[256], fu[256], msg[256];
    int         line, li;
    
    fail_unless(trace_context_format(cid, "[%W] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0); file = __FILE__, line = __LINE__, function = __FUNCTION__;
    fail_unless(fscanf(stdtrc,
                       "[%[a-zA-Z_0-9]@%[a-zA-Z_0-9.-]:%d] %[a-zA-Z. ]",
                       fu, fi, &li, msg) > 0);
    fail_unless(!strcmp(fu, function));
    fail_unless(!strcmp(fi, file));
    fail_unless(li == line);
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(function)
{
    const char *function;
    char        fu[256], msg[256];
    
    fail_unless(trace_context_format(cid, "[%C] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0); function=__FUNCTION__;
    fail_unless(fscanf(stdtrc, "[%[a-zA-Z_0-9]] %[a-zA-Z. ]", fu, msg) > 0);
    fail_unless(!strcmp(fu, function));
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(file)
{
    const char *file;
    char        fi[256], msg[256];
    
    fail_unless(trace_context_format(cid, "[%F] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0); file=__FILE__;
    fail_unless(fscanf(stdtrc, "[%[a-zA-Z_0-9.-]] %[a-zA-Z. ]", fi, msg) > 0);
    fail_unless(!strcmp(fi, file));
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(line)
{
    char msg[256];
    int  line, li;
    
    fail_unless(trace_context_format(cid, "[%L] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0); line=__LINE__;
    fail_unless(fscanf(stdtrc, "[%d] %[a-zA-Z. ]", &li, msg) > 0);
    fail_unless(li == line);
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(abs_stamp)
{
    char month[8], msg[256];
    int  year, day, hour, min, sec, msec;
    
    fail_unless(trace_context_format(cid, "[%U] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0);

    fail_unless(fscanf(stdtrc,
                       "[%u-%[A-Za-z]-%u %u:%u:%u.%u] %[a-zA-Z. ]",
                       &year, month, &day, &hour, &min, &sec, &msec, msg) > 0);
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(delta_stamp)
{
#define FIRST_MESSAGE "First message..."
    char month[8], msg[256];
    int  year, day, hour, min, sec, msec;
    
    fail_unless(trace_context_format(cid, "[%u] %M") == 0);

    fail_unless(trace_printf(DBG_TEST, "%s", FIRST_MESSAGE) > 0);
    fail_unless(fscanf(stdtrc,
                       "[%u-%[A-Za-z]-%u %u:%u:%u.%u] %[a-zA-Z. ]\n",
                       &year, month, &day, &hour, &min, &sec, &msec, msg) > 0);
    fail_unless(!strcmp(msg, FIRST_MESSAGE));

    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0);
    fail_unless(fscanf(stdtrc, "[+%u.%u] %[a-zA-Z. ]", &sec, &msec, msg) > 0);
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(message)
{
    char msg[256];
    
    fail_unless(trace_context_format(cid, "%M") == 0);
    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0);
    
    fail_unless(fscanf(stdtrc, "%[a-zA-Z. ]", msg) > 0);
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


START_TEST(and_one_more)
{
    char msg[256], ctx[32], mod[32], flag[32], month[32];
    int  year, day, hour, min, sec, msec;
    
    fail_unless(trace_context_format(cid, "%c.%m.%f emitted at %U %M") == 0);
    fail_unless(trace_printf(DBG_TEST, "%s", TEST_MESSAGE) > 0);
    
    fail_unless(fscanf(stdtrc,
                       "%[a-z0-9-].%[a-z0-9-].%[a-z0-9-] emitted at "
                       "%u-%[A-Za-z]-%u %u:%u:%u.%u %[a-zA-Z. ]",
                       ctx, mod, flag, &year, month, &day,
                       &hour, &min, &sec, &msec, msg) > 0);
    fail_unless(!strcmp(msg, TEST_MESSAGE));
}
END_TEST


void
chktrace_default_tests(Suite *suite)
{
    TCase *tc;

    tc = tcase_create("trace default context");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, context_name);
    tcase_add_test(tc, module_name);
    tcase_add_test(tc, flag_name);
    tcase_add_test(tc, where);
    tcase_add_test(tc, function);
    tcase_add_test(tc, file);
    tcase_add_test(tc, line);
    tcase_add_test(tc, abs_stamp);
    tcase_add_test(tc, delta_stamp);
    tcase_add_test(tc, message);
    tcase_add_test(tc, and_one_more);

    suite_add_tcase(suite, tc);
}



/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */


