#include "libcapslock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPSLOCK_DEFAULT_NODE_CAPACITY 4096U
#define CAPSLOCK_ALLOCATION_SLOTS 257U
#define CAPSLOCK_SHADOW_SLOTS 1021U

struct capslock_node {
    struct capslock_node *parent;
    struct capslock_node *child;
    struct capslock_node *sibling;
    uintptr_t base;
    uintptr_t end;
    capslock_permission_t permission;
    capslock_node_type_t type;
    size_t id;
};

struct allocation_slot {
    bool used;
    uintptr_t base;
    uintptr_t end;
    capslock_node_t *root;
};

struct shadow_slot {
    bool used;
    uintptr_t address;
    capslock_node_t *node;
};

struct capslock_runtime {
    capslock_node_t *nodes;
    size_t node_capacity;
    size_t node_count;
    struct allocation_slot allocations[CAPSLOCK_ALLOCATION_SLOTS];
    struct shadow_slot shadow[CAPSLOCK_SHADOW_SLOTS];
    char last_error[192];
};

static void set_error(capslock_runtime_t *runtime, const char *message)
{
    if (runtime == NULL) {
        return;
    }
    (void)snprintf(runtime->last_error, sizeof(runtime->last_error), "%s", message);
}

static size_t hash_address(uintptr_t value, size_t modulus)
{
    uint64_t mixed = (uint64_t)value;
    mixed ^= mixed >> 33;
    mixed *= UINT64_C(0xff51afd7ed558ccd);
    mixed ^= mixed >> 33;
    return (size_t)(mixed % modulus);
}

static bool range_valid(uintptr_t base, uintptr_t end)
{
    return base < end;
}

static bool range_contains(
    uintptr_t outer_base,
    uintptr_t outer_end,
    uintptr_t inner_base,
    uintptr_t inner_end)
{
    return range_valid(inner_base, inner_end) &&
           outer_base <= inner_base && inner_end <= outer_end;
}

static bool range_overlaps(
    uintptr_t left_base,
    uintptr_t left_end,
    uintptr_t right_base,
    uintptr_t right_end)
{
    return left_base < right_end && right_base < left_end;
}

static bool owns_node(
    const capslock_runtime_t *runtime, const capslock_node_t *node)
{
    return runtime != NULL && node != NULL &&
           node >= runtime->nodes && node < runtime->nodes + runtime->node_count;
}

static capslock_node_t *allocate_node(capslock_runtime_t *runtime)
{
    capslock_node_t *node;

    if (runtime->node_count == runtime->node_capacity) {
        set_error(runtime, "node arena exhausted");
        return NULL;
    }
    node = &runtime->nodes[runtime->node_count];
    (void)memset(node, 0, sizeof(*node));
    node->id = runtime->node_count;
    runtime->node_count += 1U;
    return node;
}

capslock_runtime_t *capslock_runtime_new(size_t node_capacity)
{
    capslock_runtime_t *runtime;

    if (node_capacity == 0U) {
        node_capacity = CAPSLOCK_DEFAULT_NODE_CAPACITY;
    }
    runtime = calloc(1U, sizeof(*runtime));
    if (runtime == NULL) {
        return NULL;
    }
    runtime->nodes = calloc(node_capacity, sizeof(*runtime->nodes));
    if (runtime->nodes == NULL) {
        free(runtime);
        return NULL;
    }
    runtime->node_capacity = node_capacity;
    set_error(runtime, "ok");
    return runtime;
}

void capslock_runtime_free(capslock_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }
    free(runtime->nodes);
    free(runtime);
}

capslock_node_t *capslock_create(
    capslock_runtime_t *runtime, uintptr_t base, uintptr_t end)
{
    size_t start;
    size_t offset;
    capslock_node_t *root;

    if (runtime == NULL || !range_valid(base, end)) {
        set_error(runtime, "invalid allocation range");
        return NULL;
    }
    for (offset = 0U; offset < CAPSLOCK_ALLOCATION_SLOTS; ++offset) {
        const struct allocation_slot *slot = &runtime->allocations[offset];
        if (slot->used && range_overlaps(base, end, slot->base, slot->end)) {
            set_error(runtime, "allocation overlaps an existing allocation");
            return NULL;
        }
    }

    start = hash_address(base, CAPSLOCK_ALLOCATION_SLOTS);
    for (offset = 0U; offset < CAPSLOCK_ALLOCATION_SLOTS; ++offset) {
        size_t index = (start + offset) % CAPSLOCK_ALLOCATION_SLOTS;
        struct allocation_slot *slot = &runtime->allocations[index];
        if (!slot->used) {
            root = allocate_node(runtime);
            if (root == NULL) {
                return NULL;
            }
            root->base = base;
            root->end = end;
            root->permission = CAPSLOCK_RW;
            root->type = CAPSLOCK_REF;
            slot->used = true;
            slot->base = base;
            slot->end = end;
            slot->root = root;
            set_error(runtime, "ok");
            return root;
        }
    }
    set_error(runtime, "allocation hash table exhausted");
    return NULL;
}

capslock_node_t *capslock_find_allocation(
    const capslock_runtime_t *runtime, uintptr_t address)
{
    size_t index;

    if (runtime == NULL) {
        return NULL;
    }
    for (index = 0U; index < CAPSLOCK_ALLOCATION_SLOTS; ++index) {
        const struct allocation_slot *slot = &runtime->allocations[index];
        if (slot->used && slot->base <= address && address < slot->end) {
            return slot->root;
        }
    }
    return NULL;
}

capslock_node_t *capslock_borrow(
    capslock_runtime_t *runtime,
    capslock_node_t *parent,
    bool is_mutable,
    uintptr_t base,
    uintptr_t end)
{
    capslock_node_t *node;

    if (!owns_node(runtime, parent) || parent->permission == CAPSLOCK_NA) {
        set_error(runtime, "attempting to borrow from an invalid capability");
        return NULL;
    }
    if (!range_contains(parent->base, parent->end, base, end)) {
        set_error(runtime, "borrow is outside parent bounds");
        return NULL;
    }
    if (is_mutable && parent->permission != CAPSLOCK_RW) {
        set_error(runtime, "mutable borrow requires RW permission");
        return NULL;
    }

    node = allocate_node(runtime);
    if (node == NULL) {
        return NULL;
    }
    node->parent = parent;
    node->sibling = parent->child;
    parent->child = node;
    node->base = base;
    node->end = end;
    node->permission = is_mutable ? CAPSLOCK_RW : CAPSLOCK_RO;
    node->type = CAPSLOCK_REF;
    set_error(runtime, "ok");
    return node;
}

static void invalidate_subtree(capslock_node_t *node)
{
    capslock_node_t *child;

    if (node == NULL || node->permission == CAPSLOCK_NA) {
        return;
    }
    node->permission = CAPSLOCK_NA;
    for (child = node->child; child != NULL; child = child->sibling) {
        invalidate_subtree(child);
    }
}

static void invalidate_conflicting_children(
    capslock_node_t *parent,
    const capslock_node_t *except,
    uintptr_t base,
    uintptr_t end,
    bool is_write)
{
    capslock_node_t **link = &parent->child;

    while (*link != NULL) {
        capslock_node_t *child = *link;
        bool conflicts;

        if (child->permission == CAPSLOCK_NA) {
            *link = child->sibling;
            continue;
        }
        if (child == except ||
            !range_overlaps(base, end, child->base, child->end)) {
            link = &child->sibling;
            continue;
        }

        conflicts = is_write || child->permission == CAPSLOCK_RW;
        if (!conflicts) {
            link = &child->sibling;
            continue;
        }

        if (child->type == CAPSLOCK_RAW) {
            /*
             * A raw-pointer capability remains live for stores made through
             * another capability derived from its parent. References below
             * the raw node keep normal exclusivity and are still checked.
             */
            invalidate_conflicting_children(child, NULL, base, end, is_write);
            link = &child->sibling;
            continue;
        }

        invalidate_subtree(child);
        *link = child->sibling;
    }
}

bool capslock_access(
    capslock_runtime_t *runtime,
    capslock_node_t *node,
    uintptr_t base,
    uintptr_t end,
    bool is_write)
{
    capslock_node_t *current;

    if (!owns_node(runtime, node) || node->permission == CAPSLOCK_NA) {
        set_error(runtime, "attempting to use an invalid capability");
        return false;
    }
    if (!range_contains(node->base, node->end, base, end)) {
        set_error(runtime, "access is outside capability bounds");
        return false;
    }
    if (is_write && node->permission != CAPSLOCK_RW) {
        set_error(runtime, "store requires RW permission");
        return false;
    }

    /* Descendants are not ancestors of the accessing capability. */
    invalidate_conflicting_children(node, NULL, base, end, is_write);

    current = node;
    while (current->parent != NULL) {
        capslock_node_t *parent;

        /* Stores originating inside UnsafeCell do not escape its subtree. */
        if (is_write && current->type == CAPSLOCK_UNSAFECELL) {
            break;
        }
        parent = current->parent;
        invalidate_conflicting_children(parent, current, base, end, is_write);
        current = parent;
    }

    set_error(runtime, "ok");
    return true;
}

bool capslock_revoke(capslock_runtime_t *runtime, capslock_node_t *node)
{
    capslock_node_t **link;

    if (!owns_node(runtime, node) || node->permission == CAPSLOCK_NA) {
        set_error(runtime, "attempting to revoke an invalid capability");
        return false;
    }
    invalidate_subtree(node);
    if (node->parent != NULL) {
        link = &node->parent->child;
        while (*link != NULL && *link != node) {
            link = &(*link)->sibling;
        }
        if (*link == node) {
            *link = node->sibling;
        }
    }
    set_error(runtime, "ok");
    return true;
}

bool capslock_mark_type(capslock_node_t *node, capslock_node_type_t type)
{
    if (node == NULL || node->permission == CAPSLOCK_NA ||
        type < CAPSLOCK_REF || type > CAPSLOCK_UNSAFECELL) {
        return false;
    }
    node->type = type;
    return true;
}

bool capslock_shadow_store(
    capslock_runtime_t *runtime,
    uintptr_t shadow_address,
    capslock_node_t *node)
{
    size_t start;
    size_t offset;

    if (!owns_node(runtime, node)) {
        set_error(runtime, "shadow store received an unknown capability");
        return false;
    }
    start = hash_address(shadow_address, CAPSLOCK_SHADOW_SLOTS);
    for (offset = 0U; offset < CAPSLOCK_SHADOW_SLOTS; ++offset) {
        size_t index = (start + offset) % CAPSLOCK_SHADOW_SLOTS;
        struct shadow_slot *slot = &runtime->shadow[index];
        if (!slot->used || slot->address == shadow_address) {
            slot->used = true;
            slot->address = shadow_address;
            slot->node = node;
            set_error(runtime, "ok");
            return true;
        }
    }
    set_error(runtime, "shadow hash table exhausted");
    return false;
}

capslock_node_t *capslock_shadow_load(
    const capslock_runtime_t *runtime, uintptr_t shadow_address)
{
    size_t start;
    size_t offset;

    if (runtime == NULL) {
        return NULL;
    }
    start = hash_address(shadow_address, CAPSLOCK_SHADOW_SLOTS);
    for (offset = 0U; offset < CAPSLOCK_SHADOW_SLOTS; ++offset) {
        size_t index = (start + offset) % CAPSLOCK_SHADOW_SLOTS;
        const struct shadow_slot *slot = &runtime->shadow[index];
        if (!slot->used) {
            return NULL;
        }
        if (slot->address == shadow_address) {
            return slot->node;
        }
    }
    return NULL;
}

capslock_permission_t capslock_permission(const capslock_node_t *node)
{
    return node == NULL ? CAPSLOCK_NA : node->permission;
}

capslock_node_type_t capslock_type(const capslock_node_t *node)
{
    return node == NULL ? CAPSLOCK_REF : node->type;
}

uintptr_t capslock_base(const capslock_node_t *node)
{
    return node == NULL ? 0U : node->base;
}

uintptr_t capslock_end(const capslock_node_t *node)
{
    return node == NULL ? 0U : node->end;
}

size_t capslock_id(const capslock_node_t *node)
{
    return node == NULL ? SIZE_MAX : node->id;
}

const char *capslock_last_error(const capslock_runtime_t *runtime)
{
    return runtime == NULL ? "runtime is null" : runtime->last_error;
}
