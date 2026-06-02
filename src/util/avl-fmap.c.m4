define(_M_AVL, hdql_fmap)dnl
define(_M_AVLNode, AVLNode_fmap)dnl
dnl
define(_M_isMap, true)dnl
dnl
define(_M_isKeyFixed, `true')dnl
define(_M_keyType, `void *')dnl
dnl
include(src/util/avl.m4)
