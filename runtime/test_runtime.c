#include "libcapslock.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return false;                                                       \
        }                                                                       \
    } while (0)

static capslock_runtime_t *new_runtime(void)
{
    capslock_runtime_t *runtime = capslock_runtime_new(128U);
    if (runtime == NULL) {
        fputs("unable to allocate CapsLock runtime\n", stderr);
        exit(EXIT_FAILURE);
    }
    return runtime;
}

static bool test_create_and_shadow(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x1000U, 0x1100U);

    CHECK(root != NULL);
    CHECK(capslock_permission(root) == CAPSLOCK_RW);
    CHECK(capslock_find_allocation(runtime, 0x1080U) == root);
    CHECK(capslock_find_allocation(runtime, 0x1100U) == NULL);
    CHECK(capslock_shadow_store(runtime, 0x8000U, root));
    CHECK(capslock_shadow_load(runtime, 0x8000U) == root);
    capslock_runtime_free(runtime);
    return true;
}

static bool test_borrow_permissions_and_bounds(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x2000U, 0x2100U);
    capslock_node_t *read_ref = capslock_borrow(
        runtime, root, false, 0x2020U, 0x2040U);

    CHECK(read_ref != NULL);
    CHECK(capslock_permission(read_ref) == CAPSLOCK_RO);
    CHECK(capslock_borrow(runtime, read_ref, true, 0x2020U, 0x2030U) == NULL);
    CHECK(capslock_borrow(runtime, root, true, 0x1ff0U, 0x2030U) == NULL);
    capslock_runtime_free(runtime);
    return true;
}

static bool test_nested_borrow_and_revoke(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x3000U, 0x3100U);
    capslock_node_t *level_one = capslock_borrow(
        runtime, root, true, 0x3020U, 0x3080U);
    capslock_node_t *level_two = capslock_borrow(
        runtime, level_one, false, 0x3030U, 0x3040U);

    CHECK(level_two != NULL);
    CHECK(capslock_access(runtime, level_two, 0x3030U, 0x3031U, false));
    CHECK(capslock_revoke(runtime, level_one));
    CHECK(!capslock_access(runtime, level_two, 0x3030U, 0x3031U, false));
    printf("nested borrow chain: accepted\n");
    printf("use-after-revoke: rejected (%s)\n", capslock_last_error(runtime));
    capslock_runtime_free(runtime);
    return true;
}

static bool test_store_kills_aliasing_subtree(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x4000U, 0x4100U);
    capslock_node_t *left = capslock_borrow(runtime, root, true, 0x4000U, 0x4080U);
    capslock_node_t *right = capslock_borrow(runtime, root, true, 0x4040U, 0x40c0U);
    capslock_node_t *right_child = capslock_borrow(
        runtime, right, false, 0x4050U, 0x4060U);

    CHECK(capslock_access(runtime, left, 0x4050U, 0x4051U, true));
    CHECK(capslock_permission(right) == CAPSLOCK_NA);
    CHECK(capslock_permission(right_child) == CAPSLOCK_NA);
    capslock_runtime_free(runtime);
    return true;
}

static bool test_load_only_kills_mutable_alias(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x5000U, 0x5100U);
    capslock_node_t *reader_one = capslock_borrow(
        runtime, root, false, 0x5000U, 0x5080U);
    capslock_node_t *writer = capslock_borrow(runtime, root, true, 0x5040U, 0x50c0U);
    capslock_node_t *reader_two = capslock_borrow(
        runtime, root, false, 0x5040U, 0x50c0U);

    CHECK(capslock_access(runtime, reader_one, 0x5050U, 0x5051U, false));
    CHECK(capslock_permission(writer) == CAPSLOCK_NA);
    CHECK(capslock_permission(reader_two) == CAPSLOCK_RO);
    capslock_runtime_free(runtime);
    return true;
}

static bool test_disjoint_partial_borrows(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x6000U, 0x6100U);
    capslock_node_t *left = capslock_borrow(runtime, root, true, 0x6000U, 0x6080U);
    capslock_node_t *right = capslock_borrow(runtime, root, true, 0x6080U, 0x6100U);

    CHECK(capslock_access(runtime, left, 0x6010U, 0x6011U, true));
    CHECK(capslock_permission(right) == CAPSLOCK_RW);
    CHECK(capslock_access(runtime, right, 0x6090U, 0x6091U, true));
    capslock_runtime_free(runtime);
    return true;
}

static bool test_raw_differs_from_ref(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x7000U, 0x7100U);
    capslock_node_t *raw = capslock_borrow(runtime, root, true, 0x7000U, 0x7100U);
    capslock_node_t *reference = capslock_borrow(
        runtime, root, true, 0x7000U, 0x7100U);
    capslock_node_t *writer = capslock_borrow(runtime, root, true, 0x7000U, 0x7100U);

    CHECK(capslock_mark_type(raw, CAPSLOCK_RAW));
    CHECK(capslock_access(runtime, writer, 0x7000U, 0x7001U, true));
    CHECK(capslock_permission(raw) == CAPSLOCK_RW);
    CHECK(capslock_permission(reference) == CAPSLOCK_NA);
    CHECK(capslock_access(runtime, raw, 0x7000U, 0x7001U, false));
    capslock_runtime_free(runtime);
    return true;
}

static bool test_unsafecell_store_does_not_escape(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x8000U, 0x8100U);
    capslock_node_t *outside = capslock_borrow(
        runtime, root, false, 0x8000U, 0x8100U);
    capslock_node_t *cell = capslock_borrow(runtime, root, true, 0x8000U, 0x8100U);
    capslock_node_t *inside = capslock_borrow(
        runtime, cell, true, 0x8000U, 0x8100U);

    CHECK(capslock_mark_type(cell, CAPSLOCK_UNSAFECELL));
    CHECK(capslock_access(runtime, inside, 0x8000U, 0x8001U, true));
    CHECK(capslock_permission(outside) == CAPSLOCK_RO);
    CHECK(capslock_access(runtime, outside, 0x8000U, 0x8001U, false));
    capslock_runtime_free(runtime);
    return true;
}

static bool test_access_checks(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, 0x9000U, 0x9010U);
    capslock_node_t *reader = capslock_borrow(runtime, root, false, 0x9000U, 0x9010U);

    CHECK(!capslock_access(runtime, reader, 0x9000U, 0x9001U, true));
    CHECK(!capslock_access(runtime, root, 0x8fffU, 0x9001U, false));
    capslock_runtime_free(runtime);
    return true;
}

struct test_case {
    const char *name;
    bool (*run)(void);
};

int main(void)
{
    const struct test_case tests[] = {
        {"create and shadow map", test_create_and_shadow},
        {"borrow permissions and bounds", test_borrow_permissions_and_bounds},
        {"nested borrow and revoke", test_nested_borrow_and_revoke},
        {"store invalidation", test_store_kills_aliasing_subtree},
        {"load invalidation", test_load_only_kills_mutable_alias},
        {"disjoint partial borrows", test_disjoint_partial_borrows},
        {"RAW versus REF", test_raw_differs_from_ref},
        {"UnsafeCell relaxation", test_unsafecell_store_does_not_escape},
        {"access checks", test_access_checks},
    };
    size_t index;
    size_t passed = 0U;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        bool ok = tests[index].run();
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", tests[index].name);
        passed += ok ? 1U : 0U;
    }
    printf("%zu/%zu runtime tests passed\n", passed, sizeof(tests) / sizeof(tests[0]));
    return passed == sizeof(tests) / sizeof(tests[0]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
