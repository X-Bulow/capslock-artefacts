#ifndef LIBCAPSLOCK_H
#define LIBCAPSLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct capslock_runtime capslock_runtime_t;
typedef struct capslock_node capslock_node_t;

typedef enum {
    CAPSLOCK_NA = 0,
    CAPSLOCK_RO = 1,
    CAPSLOCK_RW = 2
} capslock_permission_t;

typedef enum {
    CAPSLOCK_REF = 0,
    CAPSLOCK_RAW = 1,
    CAPSLOCK_UNSAFECELL = 2
} capslock_node_type_t;

capslock_runtime_t *capslock_runtime_new(size_t node_capacity);
void capslock_runtime_free(capslock_runtime_t *runtime);

capslock_node_t *capslock_create(
    capslock_runtime_t *runtime, uintptr_t base, uintptr_t end);
capslock_node_t *capslock_find_allocation(
    const capslock_runtime_t *runtime, uintptr_t address);

capslock_node_t *capslock_borrow(
    capslock_runtime_t *runtime,
    capslock_node_t *parent,
    bool is_mutable,
    uintptr_t base,
    uintptr_t end);

bool capslock_access(
    capslock_runtime_t *runtime,
    capslock_node_t *node,
    uintptr_t base,
    uintptr_t end,
    bool is_write);

bool capslock_revoke(capslock_runtime_t *runtime, capslock_node_t *node);
bool capslock_mark_type(capslock_node_t *node, capslock_node_type_t type);

bool capslock_shadow_store(
    capslock_runtime_t *runtime,
    uintptr_t shadow_address,
    capslock_node_t *node);
capslock_node_t *capslock_shadow_load(
    const capslock_runtime_t *runtime, uintptr_t shadow_address);

capslock_permission_t capslock_permission(const capslock_node_t *node);
capslock_node_type_t capslock_type(const capslock_node_t *node);
uintptr_t capslock_base(const capslock_node_t *node);
uintptr_t capslock_end(const capslock_node_t *node);
size_t capslock_id(const capslock_node_t *node);
const char *capslock_last_error(const capslock_runtime_t *runtime);

#endif
