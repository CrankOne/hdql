dnl Declarations in C/C++ struct
pushdef(beginStruct, `struct $1 {')dnl
pushdef(scalarAtomic, `    $2 $1;')dnl
pushdef(arrayAtomic, `    $2 $1[$3];')dnl
pushdef(scalarCompound, `    std::shared_ptr<$2> $1;')dnl
pushdef(mapCompound, `    std::unordered_map<$3, std::shared_ptr<hdql::test::$2>> $1;')dnl
pushdef(arrayCompound, `    std::vector<std::shared_ptr<$2>> $1;')dnl
pushdef(endStruct, `}; /*struct $1*/')dnl
