struct _`'TYPE`'_`'ATTR_NAME`'_Iterator {
    const hdql::test::TYPE * ownerPtr;
    decltype(hdql::test::TYPE::ATTR_NAME)::const_iterator it;
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
    if(it.ownerPtr->ATTR_NAME.end() == it.it) return NULL;
    if(keyPtr) {
        *reinterpret_cast<hdql::test::KEY_TYPE*>(keyPtr->pl.datum) = it.it->first;
    }
    return reinterpret_cast<hdql_Datum_t>(it.it->second.get());
}

static hdql_It_t
_`'TYPE`'_`'ATTR_NAME`'_advance( hdql_It_t it_ ) {
    assert(it_);
    auto & it    = *reinterpret_cast<_`'TYPE`'_`'ATTR_NAME`'_Iterator*>(it_);
    hdql::test::SimpleSelect * selection = it.selectionArgs
                 ? reinterpret_cast<hdql::test::SimpleSelect*>(it.selectionArgs)
                 : NULL
                 ;
    assert(it.ownerPtr->ATTR_NAME.end() != it.it);
    do {
        ++it.it;
    } while( selection
         && ( it.it != it.ownerPtr->ATTR_NAME.end()
          && (it.it->first < selection->nMin || it.it->first > selection->nMax)));
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
    auto & it = *reinterpret_cast<_`'TYPE`'_`'ATTR_NAME`'_Iterator*>(it_);
    hdql::test::SimpleSelect * selection = it.selectionArgs
                 ? reinterpret_cast<hdql::test::SimpleSelect*>(it.selectionArgs)
                 : NULL
                 ;
    it.it = it.ownerPtr->ATTR_NAME.begin();
    if(selection) {
        while( it.it != it.ownerPtr->ATTR_NAME.end()
            && (selection->nMin > it.it->first || selection->nMax < it.it->first)) {
            ++it.it;
        }
    }
    return reinterpret_cast<hdql_It_t>(&it);
}

static void
_`'TYPE`'_`'ATTR_NAME`'_destroy( hdql_It_t it_
              , hdql_Context_t
              ) {
    delete reinterpret_cast<_`'TYPE`'_`'ATTR_NAME`'_Iterator*>(it_);
}

const struct hdql_CollectionAttrInterface _`'TYPE`'_`'ATTR_NAME`'_iface = {
    .create = _`'TYPE`'_`'ATTR_NAME`'_create,
    .dereference = _`'TYPE`'_`'ATTR_NAME`'_dereference,
    .advance = _`'TYPE`'_`'ATTR_NAME`'_advance,
    .reset = _`'TYPE`'_`'ATTR_NAME`'_reset,
    .destroy = _`'TYPE`'_`'ATTR_NAME`'_destroy,
    .compile_selection = hdql::test::_compile_simple_selection,
    .free_selection = hdql::test::_free_simple_selection
};

