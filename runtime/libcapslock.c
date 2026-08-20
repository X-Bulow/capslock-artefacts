#include "libcapslock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPSLOCK_DEFAULT_NODE_CAPACITY 4096U
#define CAPSLOCK_ALLOCATION_SLOTS 257U
#define CAPSLOCK_SHADOW_SLOTS 1021U

struct child_index_entry;

struct capslock_node {
    struct capslock_node *parent;
    struct capslock_node *child;
    struct capslock_node *sibling;
    struct capslock_node *previous_sibling;
    struct child_index_entry *children_index;
    struct child_index_entry *index_entry;
    uintptr_t base;
    uintptr_t end;
    capslock_permission_t permission;
    capslock_node_type_t type;
    size_t id;
};

struct child_index_entry {
    struct child_index_entry *left;
    struct child_index_entry *right;
    capslock_node_t *node;
    uintptr_t maximum_end;
    uint64_t priority;
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
    struct child_index_entry *child_index_entries;
    capslock_node_t **access_scratch;
    size_t node_capacity;
    size_t node_count;
    size_t access_scratch_count;
    size_t last_access_nodes_visited;
    size_t last_access_nodes_destroyed;
    size_t last_access_index_probes;
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

static uint64_t index_priority(size_t id)
{
    uint64_t mixed = (uint64_t)id + UINT64_C(0x9e3779b97f4a7c15);
    mixed = (mixed ^ (mixed >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    mixed = (mixed ^ (mixed >> 27)) * UINT64_C(0x94d049bb133111eb);
    return mixed ^ (mixed >> 31);
}

static uintptr_t index_maximum_end(const struct child_index_entry *entry)
{
    return entry == NULL ? 0U : entry->maximum_end;
}

static void update_index_entry(struct child_index_entry *entry)
{
    uintptr_t maximum_end;

    if (entry == NULL) {
        return;
    }
    maximum_end = entry->node->end;
    if (index_maximum_end(entry->left) > maximum_end) {
        maximum_end = entry->left->maximum_end;
    }
    if (index_maximum_end(entry->right) > maximum_end) {
        maximum_end = entry->right->maximum_end;
    }
    entry->maximum_end = maximum_end;
}

static bool index_key_less(
    const struct child_index_entry *left,
    const struct child_index_entry *right)
{
    return left->node->base < right->node->base ||
           (left->node->base == right->node->base &&
            left->node->id < right->node->id);
}

static struct child_index_entry *rotate_index_left(
    struct child_index_entry *root)
{
    struct child_index_entry *new_root = root->right;

    root->right = new_root->left;
    new_root->left = root;
    update_index_entry(root);
    update_index_entry(new_root);
    return new_root;
}

static struct child_index_entry *rotate_index_right(
    struct child_index_entry *root)
{
    struct child_index_entry *new_root = root->left;

    root->left = new_root->right;
    new_root->right = root;
    update_index_entry(root);
    update_index_entry(new_root);
    return new_root;
}

static struct child_index_entry *insert_index_entry(
    struct child_index_entry *root,
    struct child_index_entry *entry)
{
    if (root == NULL) {
        return entry;
    }
    if (index_key_less(entry, root)) {
        root->left = insert_index_entry(root->left, entry);
        if (root->left->priority < root->priority) {
            root = rotate_index_right(root);
        }
    } else {
        root->right = insert_index_entry(root->right, entry);
        if (root->right->priority < root->priority) {
            root = rotate_index_left(root);
        }
    }
    update_index_entry(root);
    return root;
}

static struct child_index_entry *merge_index_entries(
    struct child_index_entry *left,
    struct child_index_entry *right)
{
    if (left == NULL) {
        return right;
    }
    if (right == NULL) {
        return left;
    }
    if (left->priority < right->priority) {
        left->right = merge_index_entries(left->right, right);
        update_index_entry(left);
        return left;
    }
    right->left = merge_index_entries(left, right->left);
    update_index_entry(right);
    return right;
}

static struct child_index_entry *remove_index_entry(
    struct child_index_entry *root,
    struct child_index_entry *entry)
{
    if (root == NULL) {
        return NULL;
    }
    if (root == entry) {
        struct child_index_entry *merged =
            merge_index_entries(root->left, root->right);
        root->left = NULL;
        root->right = NULL;
        root->maximum_end = root->node->end;
        return merged;
    }
    if (index_key_less(entry, root)) {
        root->left = remove_index_entry(root->left, entry);
    } else {
        root->right = remove_index_entry(root->right, entry);
    }
    update_index_entry(root);
    return root;
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

static bool owns_node(const capslock_runtime_t *runtime, const capslock_node_t *node)
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
    node->index_entry = &runtime->child_index_entries[node->id];
    node->index_entry->node = node;
    node->index_entry->priority = index_priority(node->id);
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
    runtime->child_index_entries =
        calloc(node_capacity, sizeof(*runtime->child_index_entries));
    runtime->access_scratch =
        calloc(node_capacity, sizeof(*runtime->access_scratch));
    if (runtime->child_index_entries == NULL || runtime->access_scratch == NULL) {
        free(runtime->access_scratch);
        free(runtime->child_index_entries);
        free(runtime->nodes);
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
    free(runtime->access_scratch);
    free(runtime->child_index_entries);
    free(runtime->nodes);
    free(runtime);
}

capslock_node_t *capslock_create(capslock_runtime_t *runtime, uintptr_t base, uintptr_t end)
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

capslock_node_t *capslock_find_allocation(const capslock_runtime_t *runtime, uintptr_t address)
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
    if (parent->child != NULL) {
        parent->child->previous_sibling = node;
    }
    parent->child = node;
    node->base = base;
    node->end = end;
    node->permission = is_mutable ? CAPSLOCK_RW : CAPSLOCK_RO;
    node->type = CAPSLOCK_REF;
    node->index_entry->maximum_end = node->end;
    parent->children_index =
        insert_index_entry(parent->children_index, node->index_entry);
    set_error(runtime, "ok");
    return node;
}

static void invalidate_subtree(
    capslock_runtime_t *runtime,
    capslock_node_t *node,
    bool count_access)
{
    capslock_node_t *child;

    if (node == NULL || node->permission == CAPSLOCK_NA) {
        return;
    }
    if (count_access) {
        runtime->last_access_nodes_visited += 1U;
        runtime->last_access_nodes_destroyed += 1U;
    }
    node->permission = CAPSLOCK_NA;
    for (child = node->child; child != NULL; child = child->sibling) {
        invalidate_subtree(runtime, child, count_access);
    }
}

static void detach_child(capslock_node_t *parent, capslock_node_t *child)
{
    if (child->previous_sibling != NULL) {
        child->previous_sibling->sibling = child->sibling;
    } else {
        parent->child = child->sibling;
    }
    if (child->sibling != NULL) {
        child->sibling->previous_sibling = child->previous_sibling;
    }
    parent->children_index =
        remove_index_entry(parent->children_index, child->index_entry);
    child->previous_sibling = NULL;
    child->sibling = NULL;
}

static void collect_conflicting_children(
    capslock_runtime_t *runtime,
    struct child_index_entry *entry,
    const capslock_node_t *except,
    uintptr_t base,
    uintptr_t end,
    bool is_write)
{
    capslock_node_t *child;

    if (entry == NULL || entry->maximum_end <= base) {
        return;
    }
    runtime->last_access_index_probes += 1U;
    collect_conflicting_children(
        runtime, entry->left, except, base, end, is_write);

    child = entry->node;
    if (child->base < end && child != except &&
        child->permission != CAPSLOCK_NA &&
        range_overlaps(base, end, child->base, child->end) &&
        (is_write || child->permission == CAPSLOCK_RW)) {
        runtime->access_scratch[runtime->access_scratch_count] = child;
        runtime->access_scratch_count += 1U;
    }

    if (child->base < end) {
        collect_conflicting_children(
            runtime, entry->right, except, base, end, is_write);
    }
}

static void invalidate_conflicting_children(
    capslock_runtime_t *runtime,
    capslock_node_t *parent,
    const capslock_node_t *except,
    uintptr_t base,
    uintptr_t end,
    bool is_write)
{
    size_t first = runtime->access_scratch_count;
    size_t last;
    size_t index;

    collect_conflicting_children(
        runtime, parent->children_index, except, base, end, is_write);
    last = runtime->access_scratch_count;
    for (index = first; index < last; ++index) {
        capslock_node_t *child = runtime->access_scratch[index];

        if (child->type == CAPSLOCK_RAW) {
            /*
             * A raw-pointer capability remains live for stores made through
             * another capability derived from its parent. References below
             * the raw node keep normal exclusivity and are still checked.
             */
            invalidate_conflicting_children(
                runtime, child, NULL, base, end, is_write);
            runtime->last_access_nodes_visited += 1U;
            continue;
        }

        invalidate_subtree(runtime, child, true);
        detach_child(parent, child);
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

    if (runtime != NULL) {
        runtime->last_access_nodes_visited = 0U;
        runtime->last_access_nodes_destroyed = 0U;
        runtime->last_access_index_probes = 0U;
        runtime->access_scratch_count = 0U;
    }

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
    runtime->last_access_nodes_visited = 1U;
    invalidate_conflicting_children(runtime, node, NULL, base, end, is_write);

    current = node;
    while (current->parent != NULL) {
        capslock_node_t *parent;

        /* Stores originating inside UnsafeCell do not escape its subtree. */
        if (is_write && current->type == CAPSLOCK_UNSAFECELL) {
            break;
        }
        parent = current->parent;
        invalidate_conflicting_children(
            runtime, parent, current, base, end, is_write);
        current = parent;
        runtime->last_access_nodes_visited += 1U;
    }

    set_error(runtime, "ok");
    return true;
}

bool capslock_revoke(capslock_runtime_t *runtime, capslock_node_t *node)
{
    if (!owns_node(runtime, node) || node->permission == CAPSLOCK_NA) {
        set_error(runtime, "attempting to revoke an invalid capability");
        return false;
    }
    invalidate_subtree(runtime, node, false);
    if (node->parent != NULL) {
        detach_child(node->parent, node);
    }
    set_error(runtime, "ok");
    return true;
}

size_t capslock_last_access_nodes_visited(const capslock_runtime_t *runtime)
{
    return runtime == NULL ? 0U : runtime->last_access_nodes_visited;
}

size_t capslock_last_access_nodes_destroyed(const capslock_runtime_t *runtime)
{
    return runtime == NULL ? 0U : runtime->last_access_nodes_destroyed;
}

size_t capslock_last_access_index_probes(const capslock_runtime_t *runtime)
{
    return runtime == NULL ? 0U : runtime->last_access_index_probes;
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

capslock_node_t *capslock_shadow_load(const capslock_runtime_t *runtime, uintptr_t shadow_address)
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
