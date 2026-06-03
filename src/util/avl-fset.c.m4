define(_M_AVL, hdql_fset)dnl
define(_M_AVLNode, AVLNode_fset)dnl
dnl
dnl define(_M_isMap, false)
dnl
define(_M_keyDynAlloc, `const')dnl
dnl
define(_M_keyType, `void *')dnl
define(_M_key_cmp, `memcmp($1, $2, $3)')
dnl
include(src/util/avl.m4)
