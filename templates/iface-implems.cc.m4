dnl Generates scalar atomic attribute methods for attribute named $1 of
dnl struct $2 of type $3
define(scalarAtomicAttributeMethods, `
static hdql_Datum_t
_dereference_$2_$1( hdql_Datum_t owner
    , hdql_Datum_t /*dynData*/
    , struct hdql_CollectionKey * /*keyPtr*/
    , const hdql_Datum_t /*defData*/
    , hdql_Context_t /*context*/
    ) {
    return reinterpret_cast<hdql_Datum_t>(
        &(reinterpret_cast<hdql::test::$2*>(owner)->$1));
}

static const struct hdql_ScalarAttrInterface _iface_$2_$1 = {
    .definitionData = NULL,
    .instantiate = NULL,
    .dereference = _dereference_$2_$1,
    .reset = NULL,
    .destroy = NULL,
};
')

dnl Generates interface implem for a collection of compound instances
dnl for attribute named $1 of struct $2 of type $3 with key type $4
define(mapCompound, `
struct _$2_$1_Iterator {
    const hdql::test::$2 * ownerPtr;
    decltype(hdql::test::$2::$1)::const_iterator it;
    hdql_SelectionArgs_t selectionArgs;
};

static hdql_It_t
_$2_$1_create( hdql_Datum_t owner
             , const hdql_Datum_t /*defData*/
             , hdql_SelectionArgs_t selArgs
             , hdql_Context_t /*context*/
             ) {
    auto itPtr = new _$2_$1_Iterator;
    itPtr->ownerPtr = reinterpret_cast<hdql::test::$2*>(owner);
    itPtr->selectionArgs = selArgs;
    return reinterpret_cast<hdql_It_t>(itPtr);
}

static hdql_Datum_t
_$2_$1_dereference( hdql_It_t it_
                  , struct hdql_CollectionKey * keyPtr
                  ) {
    auto & it    = *reinterpret_cast<_$2_$1_Iterator*>(it_);
    if(it.ownerPtr->$1.end() == it.it) return NULL;
    if(keyPtr) {
        *reinterpret_cast<$3*>(keyPtr->pl.datum) = it.it->first;
    }
    return reinterpret_cast<hdql_Datum_t>(it.it->second.get());
}

static hdql_It_t
_$2_$1_advance( hdql_It_t it_ ) {
    assert(it_);
    auto & it    = *reinterpret_cast<_$2_$1_Iterator*>(it_);
    hdql::test::SimpleSelect * selection = it.selectionArgs
                 ? reinterpret_cast<hdql::test::SimpleSelect*>(it.selectionArgs)
                 : NULL
                 ;
    assert(it.ownerPtr->$1.end() != it.it);
    do {
        ++it.it;
    } while( selection
         && ( it.it != it.ownerPtr->$1.end()
          && (it.it->first < selection->nMin || it.it->first > selection->nMax)));
    return reinterpret_cast<hdql_It_t>(&it);
}

static hdql_It_t
_$2_$1_reset( hdql_It_t it_
            , hdql_Datum_t newOwner
            , const hdql_Datum_t defData
            , hdql_SelectionArgs_t
            , hdql_Context_t
            ) {
    assert(it_);
    auto & it = *reinterpret_cast<_$2_$1_Iterator *>(it_);
    hdql::test::SimpleSelect * selection = it.selectionArgs
                 ? reinterpret_cast<hdql::test::SimpleSelect*>(it.selectionArgs)
                 : NULL
                 ;
    it.it = it.ownerPtr->$1.begin();
    if(selection) {
        while( it.it != it.ownerPtr->$1.end()
            && (selection->nMin > it.it->first || selection->nMax < it.it->first)) {
            ++it.it;
        }
    }
    return reinterpret_cast<hdql_It_t>(&it);
}

static void
_$2_$1_destroy( hdql_It_t it_
              , hdql_Context_t
              ) {
    delete reinterpret_cast<decltype(hdql::test::$2::$1)::iterator*>(it_);
}

const struct hdql_CollectionAttrInterface _$2_$1_iface = {
    .create = _$2_$1_create,
    .dereference = _$2_$1_dereference,
    .advance = _$2_$1_advance,
    .reset = _$2_$1_reset,
    .destroy = _$2_$1_destroy,
    .compile_selection = hdql::test::_compile_simple_selection,
    .free_selection = hdql::test::_free_simple_selection
};

')
