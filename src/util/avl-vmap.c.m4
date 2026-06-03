define(_M_AVL, hdql_vmap)dnl
define(_M_AVLNode, AVLNode_vmap)dnl
dnl
define(_M_isMap, true)dnl
dnl
define(_M_keyDynAlloc, `vardc')dnl
dnl
define(_M_keyType, `void *')dnl
define(_M_key_cmp, `m->key_cmp($1, $2)')
dnl
include(src/util/avl.m4)
