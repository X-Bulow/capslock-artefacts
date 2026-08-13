#include "libcapslock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPSLOCK_NODE_CAPACITY 4096


struct borrow_tree_node {
    struct borrow_tree_node *parent;
    struct borrow_tree_node *child;
    struct borrow_tree_node *sibling;
    uintptr_t base;
    uintptr_t end;
    capslock_permission_t permission;
    size_t id;
};