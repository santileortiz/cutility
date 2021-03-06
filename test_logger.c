/*
 * Copyright (C) 2020 Santiago León O.
 */

// These are required for the CRASH_TEST() and CRASH_TEST_AND_RUN() macros.
// They also require adding -lrt as build flag.
// TODO: How can we make this detail easily discoverable by users? Where to
// document it?
#include <sys/wait.h>
#include <sys/mman.h>

struct test_t {
    string_t output;
    string_t error;
    string_t children;

    bool children_success;

    struct test_t *next;
};

// TODO: Put these inside struct test_ctx_t while still supporting zero
// initialization of the structure.
#define TEST_NAME_WIDTH 40
#define TEST_INDENT 4

struct test_ctx_t {
    mem_pool_t pool;
    string_t result;

    string_t *error;
    bool last_test_success;

    // By default results of child tests are only shown if the parent test
    // failed. If the following is true, then all child test results will be
    // show.
    bool show_all_children;

    bool disable_colors;

    struct test_t *test_stack;

    struct test_t *test_fl;
};

void test_ctx_destroy (struct test_ctx_t *tc)
{
    str_free (&tc->result);
    mem_pool_destroy (&tc->pool);
}

GCC_PRINTF_FORMAT(2, 3)
void test_push (struct test_ctx_t *tc, char *name_format, ...)
{
    struct test_t *test;
    if (tc->test_fl == NULL) {
        LINKED_LIST_PUSH_NEW (&tc->pool, struct test_t, tc->test_stack, new_test);
        str_set_pooled (&tc->pool, &new_test->error, "");
        str_set_pooled (&tc->pool, &new_test->output, "");
        str_set_pooled (&tc->pool, &new_test->children, "");

        test = new_test;

    } else {
        test = tc->test_fl;
        tc->test_fl = tc->test_fl->next;
        test->next = NULL;

        LINKED_LIST_PUSH (tc->test_stack, test);
    }

    test->children_success = true;
    str_set (&test->children, "");
    str_set (&test->error, "");
    tc->error = &test->error;

    {
        PRINTF_INIT(name_format, name_size, vargs);
        str_maybe_grow (&test->output, name_size-1, false);
        char *dst = str_data(&test->output);
        PRINTF_SET (dst, name_size, name_format, vargs);

        str_cat_c (&test->output, " ");
        while (str_len(&test->output) < TEST_NAME_WIDTH-1) {
            str_cat_c (&test->output, ".");
        }
        str_cat_c (&test->output, " ");
    }
}

// TODO: Maybe move to common.h?
#define OPT_COLOR(runtime_condition,color_macro,string) \
    ((runtime_condition) ? color_macro(string) : string)

void test_pop (struct test_ctx_t *tc, bool success)
{
    struct test_t *curr_test = tc->test_stack;
    tc->test_stack = tc->test_stack->next;
    curr_test->next = NULL;
    tc->last_test_success = success;

    LINKED_LIST_PUSH (tc->test_fl, curr_test);

    if (tc->test_stack != NULL) {
        tc->test_stack->children_success = tc->test_stack->children_success && success;
    }

    if (success) {
        str_cat_printf (&curr_test->output, "%s\n",
                        OPT_COLOR(!tc->disable_colors, ECMA_GREEN, "OK"));
    } else {
        str_cat_printf (&curr_test->output, "%s\n",
                        OPT_COLOR(!tc->disable_colors, ECMA_RED, "FAILED"));
    }

    if (tc->show_all_children || !success) {
        str_cat_indented (&curr_test->output, &curr_test->error, TEST_INDENT);
        // TODO: Should this be indented?
        // :indented_children_cat
        str_cat (&curr_test->output, &curr_test->children);
    }

    if (tc->test_stack) {
        str_cat_indented (&tc->test_stack->children, &curr_test->output, TEST_INDENT);
    } else {
        str_cat (&tc->result, &curr_test->output);
    }
}

// This is like test_pop but determines fail/success based on the return status
// of children. If any children test failed this fails, if all of them passed
// then this passes.
void parent_test_pop (struct test_ctx_t *tc)
{
    test_pop (tc, tc->test_stack->children_success);
}

#define CRASH_SAFE_TEST_SHARED_VARIABLE_NAME "TEST_CRASH_SAFE_success"

bool __crash_safe_wait_and_output (mem_pool_t *pool, bool *success,
                                   char *stdout_fname, char *stderr_fname,
                                   string_t *result)
{
    bool res = false;

    int child_status;
    wait (&child_status);

    if (!WIFEXITED(child_status)) {
        *success = false;
        str_cat_printf (result, "Exited abnormally with status: %d\n", child_status);
    }

    if (!*success) {
        char *stdout_str = full_file_read (pool, stdout_fname, NULL);
        if (*stdout_str != '\0') {
            str_cat_c (result, ECMA_CYAN("stdout:\n"));
            str_cat_indented_c (result, stdout_str, 2);
        }

        char *stderr_str = full_file_read (pool, stderr_fname, NULL);
        if (*stderr_str != '\0') {
            str_cat_c (result, ECMA_CYAN("stderr:\n"));
            str_cat_indented_c (result, stderr_str, 2);
        }
    }

    res = *success;

    unlink (stdout_fname);
    unlink (stderr_fname);

    UNLINK_SHARED_VARIABLE_NAMED (CRASH_SAFE_TEST_SHARED_VARIABLE_NAME);

    return res;
}

#if !defined TEST_NO_SUBPROCESS
#define CRASH_TEST(SUCCESS,OUTPUT,CODE)                                                                 \
{                                                                                                       \
    char *__crash_safe_stdout_fname = "tmp_stdout";                                                     \
    char *__crash_safe_stderr_fname = "tmp_stderr";                                                     \
    mem_pool_t __crash_safe_pool = {0};                                                                 \
    NEW_SHARED_VARIABLE_NAMED (bool, __crash_safe_success, true, CRASH_SAFE_TEST_SHARED_VARIABLE_NAME); \
    if (fork() == 0) {                                                                                  \
        freopen (__crash_safe_stdout_fname, "w", stdout);                                               \
        setvbuf (stdout, NULL, _IONBF, 0);                                                              \
                                                                                                        \
        freopen (__crash_safe_stderr_fname, "w", stderr);                                               \
        setvbuf (stderr, NULL, _IONBF, 0);                                                              \
                                                                                                        \
        CODE                                                                                            \
                                                                                                        \
        exit(0);                                                                                        \
    }                                                                                                   \
    SUCCESS = __crash_safe_wait_and_output (&__crash_safe_pool, __crash_safe_success,                   \
                                            __crash_safe_stdout_fname, __crash_safe_stderr_fname,       \
                                            OUTPUT);                                                    \
}

#define CRASH_TEST_AND_RUN(SUCCESS,OUTPUT,CODE)  \
    CRASH_TEST(SUCCESS,OUTPUT,CODE)              \
    if (SUCCESS) {                               \
        CODE                                     \
    }

#else
#define CRASH_TEST(SUCCESS,OUTPUT,CODE) CODE
#define CRASH_TEST_AND_RUN(SUCCESS,OUTPUT,CODE) CODE

#endif
