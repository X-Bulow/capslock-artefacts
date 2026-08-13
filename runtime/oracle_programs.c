#include "libcapslock.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define RANGE_BASE ((uintptr_t)0x10000U)
#define RANGE_END ((uintptr_t)0x10100U)

static capslock_runtime_t *new_runtime(void)
{
    capslock_runtime_t *runtime = capslock_runtime_new(128U);
    if (runtime == NULL) {
        fputs("unable to allocate CapsLock runtime\n", stderr);
        exit(EXIT_FAILURE);
    }
    return runtime;
}

static bool report_expected_rejection(
    const char *name,
    int source_line,
    capslock_runtime_t *runtime,
    bool was_allowed)
{
    printf(
        "  expected flag at oracle_programs.c:%d: %s%s%s\n",
        source_line,
        was_allowed ? "MISS" : "PASS (",
        was_allowed ? "" : capslock_last_error(runtime),
        was_allowed ? "" : ")");
    if (was_allowed) {
        printf("  %s was not rejected\n", name);
    }
    capslock_runtime_free(runtime);
    return !was_allowed;
}

static bool listing1_use_after_free_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *buffer = capslock_create(runtime, RANGE_BASE, RANGE_END);
    bool allowed;

    if (buffer == NULL || !capslock_revoke(runtime, buffer)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(runtime, buffer, RANGE_BASE, RANGE_BASE + 1U, true);
    return report_expected_rejection(
        "Listing 1 use-after-free", __LINE__ - 2, runtime, allowed);
}

static bool listing1_use_after_free_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *buffer = capslock_create(runtime, RANGE_BASE, RANGE_END);
    bool allowed = buffer != NULL &&
        capslock_access(runtime, buffer, RANGE_BASE, RANGE_BASE + 1U, true);

    capslock_runtime_free(runtime);
    return allowed;
}

static bool listing1_data_race_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, RANGE_BASE, RANGE_END);
    capslock_node_t *thread_ref = capslock_borrow(
        runtime, root, true, RANGE_BASE, RANGE_END);
    capslock_node_t *main_ref = capslock_borrow(
        runtime, root, true, RANGE_BASE, RANGE_END);
    bool allowed;

    if (thread_ref == NULL || main_ref == NULL ||
        !capslock_access(runtime, thread_ref, RANGE_BASE + 40U, RANGE_BASE + 44U, true)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(
        runtime, main_ref, RANGE_BASE + 40U, RANGE_BASE + 44U, true);
    return report_expected_rejection(
        "Listing 1 data race", __LINE__ - 2, runtime, allowed);
}

static bool listing1_data_race_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, RANGE_BASE, RANGE_END);
    capslock_node_t *thread_ref = capslock_borrow(
        runtime, root, true, RANGE_BASE, RANGE_BASE + 0x80U);
    capslock_node_t *main_ref = capslock_borrow(
        runtime, root, true, RANGE_BASE + 0x80U, RANGE_END);
    bool allowed = thread_ref != NULL && main_ref != NULL &&
        capslock_access(runtime, thread_ref, RANGE_BASE + 40U, RANGE_BASE + 44U, true) &&
        capslock_access(runtime, main_ref, RANGE_BASE + 0xc0U, RANGE_BASE + 0xc4U, true);

    capslock_runtime_free(runtime);
    return allowed;
}

static bool listing1_unsafe_aliasing_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 16U);
    capslock_node_t *destination = capslock_borrow(
        runtime, root, true, RANGE_BASE + 2U, RANGE_BASE + 10U);
    capslock_node_t *source = capslock_borrow(
        runtime, root, false, RANGE_BASE, RANGE_BASE + 8U);
    bool allowed;

    if (destination == NULL || source == NULL ||
        !capslock_access(runtime, source, RANGE_BASE + 2U, RANGE_BASE + 3U, false)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(
        runtime, destination, RANGE_BASE + 2U, RANGE_BASE + 3U, true);
    return report_expected_rejection(
        "Listing 1 unsafe aliasing", __LINE__ - 2, runtime, allowed);
}

static bool listing1_unsafe_aliasing_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *root = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 16U);
    capslock_node_t *destination = capslock_borrow(
        runtime, root, true, RANGE_BASE + 8U, RANGE_BASE + 16U);
    capslock_node_t *source = capslock_borrow(
        runtime, root, false, RANGE_BASE, RANGE_BASE + 8U);
    bool allowed = destination != NULL && source != NULL &&
        capslock_access(runtime, source, RANGE_BASE, RANGE_BASE + 8U, false) &&
        capslock_access(runtime, destination, RANGE_BASE + 8U, RANGE_BASE + 16U, true);

    capslock_runtime_free(runtime);
    return allowed;
}

static bool figure3_raw_then_reference_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *value = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *raw = capslock_borrow(
        runtime, value, true, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *reference;
    bool allowed;

    if (raw == NULL || !capslock_mark_type(raw, CAPSLOCK_RAW)) {
        capslock_runtime_free(runtime);
        return false;
    }
    reference = capslock_borrow(runtime, raw, true, RANGE_BASE, RANGE_BASE + 8U);
    if (reference == NULL ||
        !capslock_access(runtime, raw, RANGE_BASE, RANGE_BASE + 8U, true)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(runtime, reference, RANGE_BASE, RANGE_BASE + 8U, false);
    return report_expected_rejection(
        "Figure 3 stale v_ref", __LINE__ - 2, runtime, allowed);
}

static bool figure3_raw_then_reference_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *value = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *raw = capslock_borrow(
        runtime, value, true, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *reference;
    bool allowed;

    if (raw == NULL || !capslock_mark_type(raw, CAPSLOCK_RAW)) {
        capslock_runtime_free(runtime);
        return false;
    }
    reference = capslock_borrow(runtime, raw, true, RANGE_BASE, RANGE_BASE + 8U);
    allowed = reference != NULL &&
        capslock_access(runtime, reference, RANGE_BASE, RANGE_BASE + 8U, true);
    capslock_runtime_free(runtime);
    return allowed;
}

static bool rustsec_2020_0023_mutable_alias_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *row = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *slice_one = capslock_borrow(
        runtime, row, true, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *slice_two = capslock_borrow(
        runtime, row, true, RANGE_BASE, RANGE_BASE + 8U);
    bool allowed;

    if (slice_one == NULL || slice_two == NULL ||
        !capslock_access(runtime, slice_one, RANGE_BASE, RANGE_BASE + 1U, false)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(runtime, slice_two, RANGE_BASE, RANGE_BASE + 1U, true);
    return report_expected_rejection(
        "RUSTSEC-2020-0023 mutable aliases", __LINE__ - 2, runtime, allowed);
}

static bool rustsec_2020_0023_mutable_alias_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *row = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *slice_one = capslock_borrow(
        runtime, row, true, RANGE_BASE, RANGE_BASE + 4U);
    capslock_node_t *slice_two = capslock_borrow(
        runtime, row, true, RANGE_BASE + 4U, RANGE_BASE + 8U);
    bool allowed = slice_one != NULL && slice_two != NULL &&
        capslock_access(runtime, slice_one, RANGE_BASE, RANGE_BASE + 1U, false) &&
        capslock_access(runtime, slice_two, RANGE_BASE + 4U, RANGE_BASE + 5U, true);

    capslock_runtime_free(runtime);
    return allowed;
}

static bool rustsec_2022_0002_guard_lifetime_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *map_entry = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *guard = capslock_borrow(
        runtime, map_entry, true, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *value = capslock_borrow(
        runtime, guard, false, RANGE_BASE, RANGE_BASE + 8U);
    bool allowed;

    if (value == NULL || !capslock_revoke(runtime, guard)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(runtime, value, RANGE_BASE, RANGE_BASE + 4U, false);
    return report_expected_rejection(
        "RUSTSEC-2022-0002 value outlives guard", __LINE__ - 2, runtime, allowed);
}

static bool rustsec_2022_0002_guard_lifetime_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *map_entry = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *guard = capslock_borrow(
        runtime, map_entry, true, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *value = capslock_borrow(
        runtime, guard, false, RANGE_BASE, RANGE_BASE + 8U);
    bool allowed = value != NULL &&
        capslock_access(runtime, value, RANGE_BASE, RANGE_BASE + 4U, false);

    capslock_runtime_free(runtime);
    return allowed;
}

static bool rustsec_2021_0114_tls_alias_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *tls = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *x_ref = capslock_borrow(
        runtime, tls, true, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *y_ref = capslock_borrow(
        runtime, tls, true, RANGE_BASE, RANGE_BASE + 8U);
    bool allowed;

    if (x_ref == NULL || y_ref == NULL ||
        !capslock_access(runtime, x_ref, RANGE_BASE, RANGE_BASE + 8U, true)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(runtime, y_ref, RANGE_BASE, RANGE_BASE + 8U, true);
    return report_expected_rejection(
        "RUSTSEC-2021-0114 aliased TLS RNG", __LINE__ - 2, runtime, allowed);
}

static bool rustsec_2021_0114_tls_alias_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *tls_x = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *tls_y = capslock_create(
        runtime, RANGE_BASE + 0x10U, RANGE_BASE + 0x18U);
    capslock_node_t *x_ref = capslock_borrow(
        runtime, tls_x, true, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *y_ref = capslock_borrow(
        runtime, tls_y, true, RANGE_BASE + 0x10U, RANGE_BASE + 0x18U);
    bool allowed = x_ref != NULL && y_ref != NULL &&
        capslock_access(runtime, x_ref, RANGE_BASE, RANGE_BASE + 8U, true) &&
        capslock_access(runtime, y_ref, RANGE_BASE + 0x10U, RANGE_BASE + 0x18U, true);

    capslock_runtime_free(runtime);
    return allowed;
}

static bool rustsec_2019_0009_freed_backing_store_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *backing = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 16U);
    capslock_node_t *element = capslock_borrow(
        runtime, backing, false, RANGE_BASE, RANGE_BASE + 1U);
    bool allowed;

    if (element == NULL || !capslock_revoke(runtime, backing)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(runtime, element, RANGE_BASE, RANGE_BASE + 1U, false);
    return report_expected_rejection(
        "RUSTSEC-2019-0009 freed SmallVec backing store", __LINE__ - 2,
        runtime, allowed);
}

static bool rustsec_2019_0009_freed_backing_store_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *backing = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 16U);
    capslock_node_t *element = capslock_borrow(
        runtime, backing, false, RANGE_BASE, RANGE_BASE + 1U);
    bool allowed = element != NULL &&
        capslock_access(runtime, element, RANGE_BASE, RANGE_BASE + 1U, false);

    capslock_runtime_free(runtime);
    return allowed;
}

static bool rustsec_2022_0007_read_then_clear_bug(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *vector = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 16U);
    capslock_node_t *reader = capslock_borrow(
        runtime, vector, false, RANGE_BASE, RANGE_BASE + 16U);
    capslock_node_t *writer = capslock_borrow(
        runtime, vector, true, RANGE_BASE, RANGE_BASE + 16U);
    bool allowed;

    if (reader == NULL || writer == NULL ||
        !capslock_access(runtime, writer, RANGE_BASE, RANGE_BASE + 16U, true)) {
        capslock_runtime_free(runtime);
        return false;
    }
    allowed = capslock_access(runtime, reader, RANGE_BASE, RANGE_BASE + 1U, false);
    return report_expected_rejection(
        "RUSTSEC-2022-0007 reference after clear", __LINE__ - 2, runtime, allowed);
}

static bool rustsec_2022_0007_read_then_clear_clean(void)
{
    capslock_runtime_t *runtime = new_runtime();
    capslock_node_t *vector = capslock_create(runtime, RANGE_BASE, RANGE_BASE + 16U);
    capslock_node_t *reader = capslock_borrow(
        runtime, vector, false, RANGE_BASE, RANGE_BASE + 8U);
    capslock_node_t *writer = capslock_borrow(
        runtime, vector, true, RANGE_BASE + 8U, RANGE_BASE + 16U);
    bool allowed = reader != NULL && writer != NULL &&
        capslock_access(runtime, writer, RANGE_BASE + 8U, RANGE_BASE + 16U, true) &&
        capslock_access(runtime, reader, RANGE_BASE, RANGE_BASE + 1U, false);

    capslock_runtime_free(runtime);
    return allowed;
}

struct oracle_case {
    const char *name;
    bool (*bug)(void);
    bool (*clean)(void);
};

int main(void)
{
    const struct oracle_case cases[] = {
        {"Listing 1(a): use-after-free", listing1_use_after_free_bug,
         listing1_use_after_free_clean},
        {"Listing 1(b): data race", listing1_data_race_bug,
         listing1_data_race_clean},
        {"Listing 1(c): unsafe aliasing", listing1_unsafe_aliasing_bug,
         listing1_unsafe_aliasing_clean},
        {"Figure 3: raw write invalidates ref", figure3_raw_then_reference_bug,
         figure3_raw_then_reference_clean},
        {"RUSTSEC-2020-0023: mutable aliases", rustsec_2020_0023_mutable_alias_bug,
         rustsec_2020_0023_mutable_alias_clean},
        {"RUSTSEC-2022-0002: guard lifetime", rustsec_2022_0002_guard_lifetime_bug,
         rustsec_2022_0002_guard_lifetime_clean},
        {"RUSTSEC-2021-0114: TLS aliases", rustsec_2021_0114_tls_alias_bug,
         rustsec_2021_0114_tls_alias_clean},
        {"RUSTSEC-2019-0009: freed backing store",
         rustsec_2019_0009_freed_backing_store_bug,
         rustsec_2019_0009_freed_backing_store_clean},
        {"RUSTSEC-2022-0007: reference after clear",
         rustsec_2022_0007_read_then_clear_bug,
         rustsec_2022_0007_read_then_clear_clean},
    };
    size_t index;
    size_t bug_passed = 0U;
    size_t clean_passed = 0U;

    puts("CapsLock hand-written oracle programs");
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        bool bug_ok;
        bool clean_ok;

        printf("[%zu] %s\n", index + 1U, cases[index].name);
        bug_ok = cases[index].bug();
        clean_ok = cases[index].clean();
        printf("  bug: %s; clean variant: %s\n",
               bug_ok ? "FLAGGED" : "MISS",
               clean_ok ? "PASS" : "FALSE POSITIVE");
        bug_passed += bug_ok ? 1U : 0U;
        clean_passed += clean_ok ? 1U : 0U;
    }
    printf(
        "%zu/%zu buggy cases flagged; %zu/%zu clean variants passed\n",
        bug_passed,
        sizeof(cases) / sizeof(cases[0]),
        clean_passed,
        sizeof(cases) / sizeof(cases[0]));
    return bug_passed == sizeof(cases) / sizeof(cases[0]) &&
                   clean_passed == sizeof(cases) / sizeof(cases[0])
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
