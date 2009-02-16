#ifndef __CHECK_LIBTRACE_H__
#define __CHECK_LIBTRACE_H__

#include <check.h>

void chktrace_context_tests(Suite *suite);
void chktrace_module_tests(Suite *suite);
void chktrace_flag_tests(Suite *suite);
void chktrace_format_tests(Suite *suite);
void chktrace_target_tests(Suite *suite);
int capture_fd(int fd, int *pipe_fd, int *saved_fd);
int release_fd(int fd, int *pipe_fd, int saved_fd);

SRunner *chktrace_srunner(void);

#endif /* __CHECK_LIBTRACE_H__ */
