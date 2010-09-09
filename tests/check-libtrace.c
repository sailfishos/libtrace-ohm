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
#include <unistd.h>
#include <fcntl.h>
#include <check.h>
#include <simple-trace/simple-trace.h>

#include "check-libtrace.h"


static SRunner *srunner;                       /* check suite runner */

int
capture_fd(int fd, int *pipe_fd, int *saved_fd)
{
    if (pipe(pipe_fd) < 0)
        return -1;

    fcntl(fd, F_SETFL, O_NONBLOCK);
    
    if ((*saved_fd = dup(fd)) < 0)
        return -1;

    if (dup2(pipe_fd[1], fd) < 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }
    
    fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_fd[1], F_SETFL, O_NONBLOCK);
    
    return 0;
}


int
release_fd(int fd, int *pipe_fd, int saved_fd)
{
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    
    return dup2(saved_fd, fd) >=  0 ? 0 : -1;
}



static Suite *
chktrace_suite(void)
{
    Suite *suite;

    suite = suite_create("simple-trace");

    chktrace_context_tests(suite);
    chktrace_module_tests(suite);
    chktrace_flag_tests(suite);
    chktrace_format_tests(suite);
    chktrace_default_tests(suite);
#if 1
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

