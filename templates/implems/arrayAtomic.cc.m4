struct _`'TYPE`'_`'ATTR_NAME`'_Iterator {
    hdql::test::TYPE * ownerPtr;
    size_t n;
    hdql_SelectionArgs_t selectionArgs;
};

static hdql_It_t
_`'TYPE`'_`'ATTR_NAME`'_create( hdql_Datum_t owner
             , const hdql_Datum_t /*defData*/
             , hdql_SelectionArgs_t selArgs
             , hdql_Context_t /*context*/
             ) {
    auto itPtr = new _`'TYPE`'_`'ATTR_NAME`'_Iterator;
    itPtr->ownerPtr = reinterpret_cast<hdql::test::TYPE*>(owner);
    itPtr->selectionArgs = selArgs;
    return reinterpret_cast<hdql_It_t>(itPtr);
}

static hdql_Datum_t
_`'TYPE`'_`'ATTR_NAME`'_dereference( hdql_It_t it_
                  , struct hdql_CollectionKey * keyPtr
                  ) {
    auto & it    = *reinterpret_cast<_`'TYPE`'_`'ATTR_NAME`'_Iterator*>(it_);
    if(ARRAY_SIZE == it.n) return NULL;
    if(keyPtr) {
        *reinterpret_cast<size_t*>(keyPtr->pl.datum) = it.n;
    }
    return reinterpret_cast<hdql_Datum_t>(it.ownerPtr + it.n);
}

static hdql_It_t
_`'TYPE`'_`'ATTR_NAME`'_advance( hdql_It_t it_ ) {
    assert(it_);
    auto & it    = *reinterpret_cast<_`'TYPE`'_`'ATTR_NAME`'_Iterator*>(it_);
    hdql::test::SimpleSelect * selection = it.selectionArgs
                 ? reinterpret_cast<hdql::test::SimpleSelect*>(it.selectionArgs)
                 : NULL
                 ;
    assert(ARRAY_SIZE > it.n);
    do {
        ++it.n;
    } while( selection
         && ( it.n != ARRAY_SIZE
          && ( it.n < static_cast<size_t>(selection->nMin)
            || it.n > static_cast<size_t>(selection->nMax)
             )
            )
         );
    return reinterpret_cast<hdql_It_t>(&it);
}

static hdql_It_t
_`'TYPE`'_`'ATTR_NAME`'_reset( hdql_It_t it_
            , hdql_Datum_t newOwner
            , const hdql_Datum_t defData
            , hdql_SelectionArgs_t
            , hdql_Context_t
            ) {
    assert(it_);
    auto & it = *reinterpret_cast<_`'TYPE`'_`'ATTR_NAME`'_Iterator *>(it_);
    hdql::test::SimpleSelect * selection = it.selectionArgs
                 ? reinterpret_cast<hdql::test::SimpleSelect*>(it.selectionArgs)
                 : NULL
                 ;
    it.n = 0;
    if(selection) {
        while( it.n != ARRAY_SIZE
            && (static_cast<size_t>(selection->nMin) > it.n
             || static_cast<size_t>(selection->nMax) < it.n)) {
            ++it.n;
        }
    }
    return reinterpret_cast<hdql_It_t>(&it);
}

static void
_`'TYPE`'_`'ATTR_NAME`'_destroy( hdql_It_t it_
              , hdql_Context_t
              ) {
    delete reinterpret_cast<_`'TYPE`'_`'ATTR_NAME`'_Iterator *>(it_);
}

