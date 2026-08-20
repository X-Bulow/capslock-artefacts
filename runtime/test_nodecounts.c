#include "libcapslock.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return false;                                                       \
        }                                                                       \
    } while (0)

#define REQUIRE(condition)                                                      \
    do {                                                                        \
        if (!(condition)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return EXIT_FAILURE;                                                \
        }                                                                       \
    } while (0)

struct access_counts {
    size_t visited;
    size_t destroyed;
    size_t index_probes;
};

static bool measure_wide_tree(size_t sibling_count, struct access_counts *counts)
{
    const uintptr_t base = UINT64_C(0x100000);
    const uintptr_t stride = UINT64_C(0x10);
    capslock_runtime_t *runtime = capslock_runtime_new(sibling_count + 2U);
    capslock_node_t *root;
    capslock_node_t *active;
    size_t index;

    CHECK(runtime != NULL);
    root = capslock_create(
        runtime, base, base + stride * (uintptr_t)(sibling_count + 1U));
    active = capslock_borrow(runtime, root, true, base, base + 8U);
    CHECK(root != NULL && active != NULL);

    for (index = 0U; index < sibling_count; ++index) {
        uintptr_t sibling_base = base + stride * (uintptr_t)(index + 1U);
        CHECK(capslock_borrow(
                  runtime, root, true, sibling_base, sibling_base + 8U) != NULL);
    }

    CHECK(capslock_access(runtime, active, base, base + 1U, true));
    counts->visited = capslock_last_access_nodes_visited(runtime);
    counts->destroyed = capslock_last_access_nodes_destroyed(runtime);
    counts->index_probes = capslock_last_access_index_probes(runtime);
    capslock_runtime_free(runtime);
    return true;
}

static bool measure_deep_tree(size_t depth, struct access_counts *counts)
{
    const uintptr_t base = UINT64_C(0x200000);
    capslock_runtime_t *runtime = capslock_runtime_new(depth + 1U);
    capslock_node_t *current;
    size_t index;

    CHECK(runtime != NULL);
    current = capslock_create(runtime, base, base + 8U);
    CHECK(current != NULL);
    for (index = 0U; index < depth; ++index) {
        current = capslock_borrow(runtime, current, true, base, base + 8U);
        CHECK(current != NULL);
    }

    CHECK(capslock_access(runtime, current, base, base + 1U, true));
    counts->visited = capslock_last_access_nodes_visited(runtime);
    counts->destroyed = capslock_last_access_nodes_destroyed(runtime);
    counts->index_probes = capslock_last_access_index_probes(runtime);
    capslock_runtime_free(runtime);
    return true;
}

static bool measure_destroyed_subtree(
    size_t descendant_count,
    struct access_counts *counts)
{
    const uintptr_t base = UINT64_C(0x300000);
    capslock_runtime_t *runtime = capslock_runtime_new(descendant_count + 3U);
    capslock_node_t *root;
    capslock_node_t *active;
    capslock_node_t *victim;
    size_t index;

    CHECK(runtime != NULL);
    root = capslock_create(runtime, base, base + 8U);
    active = capslock_borrow(runtime, root, true, base, base + 8U);
    victim = capslock_borrow(runtime, root, true, base, base + 8U);
    CHECK(root != NULL && active != NULL && victim != NULL);
    for (index = 0U; index < descendant_count; ++index) {
        victim = capslock_borrow(runtime, victim, true, base, base + 8U);
        CHECK(victim != NULL);
    }

    CHECK(capslock_access(runtime, active, base, base + 1U, true));
    counts->visited = capslock_last_access_nodes_visited(runtime);
    counts->destroyed = capslock_last_access_nodes_destroyed(runtime);
    counts->index_probes = capslock_last_access_index_probes(runtime);
    capslock_runtime_free(runtime);
    return true;
}

int main(void)
{
    struct access_counts wide_small;
    struct access_counts wide_large;
    struct access_counts deep;
    struct access_counts destroyed;
    const size_t deep_depth = 64U;
    const size_t destroyed_descendants = 32U;
    size_t maximum;

    if (!measure_wide_tree(8U, &wide_small) ||
        !measure_wide_tree(512U, &wide_large) ||
        !measure_deep_tree(deep_depth, &deep) ||
        !measure_destroyed_subtree(destroyed_descendants, &destroyed)) {
        return EXIT_FAILURE;
    }

    printf("wide tree (8 irrelevant siblings): visited=%zu destroyed=%zu index-probes=%zu\n",
           wide_small.visited, wide_small.destroyed, wide_small.index_probes);
    printf("wide tree (512 irrelevant siblings): visited=%zu destroyed=%zu index-probes=%zu\n",
           wide_large.visited, wide_large.destroyed, wide_large.index_probes);
    printf("deep tree (depth=%zu): visited=%zu destroyed=%zu index-probes=%zu\n",
           deep_depth, deep.visited, deep.destroyed, deep.index_probes);
    printf("destroyed subtree (depth=1, nodes=%zu): visited=%zu destroyed=%zu index-probes=%zu\n",
           destroyed_descendants + 1U, destroyed.visited, destroyed.destroyed,
           destroyed.index_probes);

    REQUIRE(wide_small.destroyed == 0U);
    REQUIRE(wide_large.destroyed == 0U);
    REQUIRE(wide_small.visited == 2U);
    REQUIRE(wide_large.visited == wide_small.visited);
    REQUIRE(wide_large.index_probes < 64U);
    REQUIRE(deep.destroyed == 0U);
    REQUIRE(deep.visited == deep_depth + 1U);
    REQUIRE(destroyed.destroyed == destroyed_descendants + 1U);
    REQUIRE(destroyed.visited == 2U + destroyed.destroyed);

    maximum = wide_small.visited;
    if (wide_large.visited > maximum) {
        maximum = wide_large.visited;
    }
    if (deep.visited > maximum) {
        maximum = deep.visited;
    }
    if (destroyed.visited > maximum) {
        maximum = destroyed.visited;
    }
    printf("maximum capability nodes visited: %zu\n", maximum);
    puts("node-visit bound: PASS");
    return EXIT_SUCCESS;
}
