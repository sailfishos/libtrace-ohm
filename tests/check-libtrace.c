#include <stdio.h>
#include <check.h>
#include <simple-trace/simple-trace.h>

#include "check-libtrace.h"


static SRunner *srunner;                       /* check suite runner */

static Suite *
chktrace_suite(void)
{
    Suite *suite;

    suite = suite_create("simple-trace");

    chktrace_context_tests(suite);
#if 0
    chktrace_module_tests(suite);
    chktrace_flag_tests(suite);
    chktrace_format_tests(suite);
    chktrace_target_tests(suite);
#endif

    return suite;
}

SRunner *
chktrace_srunner(void)
{
    return srunner;
}


int
main(int argc, char *argv[])
{
    Suite *suite;
    int    nfailed;

    (void)argc;
    (void)argv;

    suite   = chktrace_suite();
    srunner = srunner_create(suite);

    srunner_run_all(srunner, CK_ENV);
    nfailed = srunner_ntests_failed(srunner);
    
    srunner_free(srunner);

    return nfailed == 0 ? 0 : 1;
}



/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */

