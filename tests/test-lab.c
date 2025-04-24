#include "harness/unity.h"
#include "../src/lab.h"

// NOTE: These single-threaded tests exercise basic invariants.
// The provided CLI tester is still required for full multi-thread coverage.

void setUp(void)   {}
void tearDown(void){}

void test_create_destroy(void)
{
    queue_t q = queue_init(10);
    TEST_ASSERT_NOT_NULL(q);
    queue_destroy(q);
}

void test_queue_dequeue(void)
{
    queue_t q = queue_init(10);
    int data = 1;
    enqueue(q, &data);
    TEST_ASSERT_EQUAL_PTR(&data, dequeue(q));
    queue_destroy(q);
}

void test_queue_dequeue_multiple(void)
{
    queue_t q = queue_init(10);
    int d1 = 1, d2 = 2, d3 = 3;
    enqueue(q, &d1); enqueue(q, &d2); enqueue(q, &d3);
    TEST_ASSERT_EQUAL_PTR(&d1, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&d2, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&d3, dequeue(q));
    queue_destroy(q);
}

void test_queue_dequeue_shutdown(void)
{
    queue_t q = queue_init(10);
    int d1 = 1, d2 = 2, d3 = 3;
    enqueue(q, &d1); enqueue(q, &d2); enqueue(q, &d3);
    TEST_ASSERT_EQUAL_PTR(&d1, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&d2, dequeue(q));
    queue_shutdown(q);
    TEST_ASSERT_EQUAL_PTR(&d3, dequeue(q));
    TEST_ASSERT_TRUE(is_shutdown(q));
    TEST_ASSERT_TRUE(is_empty(q));
    queue_destroy(q);
}

/* ───────────────────── extra tests ───────────────────── */

/* Fill->dequeue->enqueue to ensure internal indices wrap correctly */
void test_wraparound(void)
{
    queue_t q = queue_init(3);
    int a = 1, b = 2, c = 3, d = 4;

    enqueue(q, &a);
    enqueue(q, &b);
    enqueue(q, &c);
    TEST_ASSERT_EQUAL_PTR(&a, dequeue(q));   // head moves → frees one slot

    enqueue(q, &d);                          // should reuse slot 0 (wrap)
    TEST_ASSERT_EQUAL_PTR(&b, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&c, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&d, dequeue(q));
    TEST_ASSERT_TRUE(is_empty(q));

    queue_destroy(q);
}

/* After shutdown, enqueue should be a no-op and dequeue returns NULL once empty */
void test_enqueue_after_shutdown(void)
{
    queue_t q = queue_init(2);
    int a = 1, b = 2;

    enqueue(q, &a);
    queue_shutdown(q);

    enqueue(q, &b);                          // should be ignored
    TEST_ASSERT_EQUAL_PTR(&a, dequeue(q));   // only original item available
    TEST_ASSERT_NULL(dequeue(q));            // queue is empty & shutdown
    queue_destroy(q);
}

/* ───────────────────── Test runner ───────────────────── */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_queue_dequeue);
    RUN_TEST(test_queue_dequeue_multiple);
    RUN_TEST(test_queue_dequeue_shutdown);

    RUN_TEST(test_wraparound);               // NEW
    RUN_TEST(test_enqueue_after_shutdown);   // NEW

    return UNITY_END();
}
