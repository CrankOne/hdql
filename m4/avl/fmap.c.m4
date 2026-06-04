define(_M_AVL, hdql_fmap)dnl
define(_M_AVLNode, AVLNode_fmap)dnl
dnl
define(_M_isMap, true)dnl
dnl
define(_M_keyDynAlloc, `const')dnl
dnl
define(_M_keyType, `void *')dnl
define(_M_key_cmp, `memcmp($1, $2, $3)')
dnl
include(avl.m4)
