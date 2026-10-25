/**
 * @file  test.h
 * @brief Minimal host-side test harness (no dependencies).
 *
 * Each test_<suite>.c defines TEST(name) functions and a run_suite()
 * that calls RUN(name) for each. Exit code = number of failed tests.
 */

#ifndef TEST_H
#define TEST_H

#include <stdio.h>

static int g_failed = 0;
static int g_run    = 0;
static int g_cur_ok = 1;

#define TEST(name) static void name(void)

#define RUN(name)                                                             \
    do                                                                        \
    {                                                                         \
        g_cur_ok = 1;                                                         \
        g_run++;                                                              \
        name();                                                               \
        if (g_cur_ok)                                                         \
        {                                                                     \
            printf("  pass  %s\n", #name);                                    \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            g_failed++;                                                       \
            printf("  FAIL  %s\n", #name);                                    \
        }                                                                     \
    } while (0)

#define CHECK(cond)                                                           \
    do                                                                        \
    {                                                                         \
        if (!(cond))                                                          \
        {                                                                     \
            printf("        %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond);  \
            g_cur_ok = 0;                                                     \
            return;                                                           \
        }                                                                     \
    } while (0)

#define CHECK_EQ(exp, act)                                                    \
    do                                                                        \
    {                                                                         \
        long long e_ = (long long)(exp);                                      \
        long long a_ = (long long)(act);                                      \
        if (e_ != a_)                                                         \
        {                                                                     \
            printf("        %s:%d: expected %s == %lld, got %lld\n",          \
                   __FILE__, __LINE__, #act, e_, a_);                         \
            g_cur_ok = 0;                                                     \
            return;                                                           \
        }                                                                     \
    } while (0)

static void run_suite(void);

int main(void)
{
    run_suite();
    printf("  %d run, %d failed\n", g_run, g_failed);
    return g_failed;
}

#endif /* TEST_H */
