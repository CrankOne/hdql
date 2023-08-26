#pragma once

#include <unordered_map>
#include <cassert>
#include <cstdlib>

#include "hdql/compound.h"
#include "hdql/query.h"
#include "hdql/types.h"

//
// C++ API and helpers

namespace hdql {

class Context;
struct SelectionArgs;

/// Defines how to deal with common STL collections
template<auto T> struct IteratorTraits;

/// Specialization for iterable items
//template< typename OwnerT
//        , typename CollectionMemberT
//        , CollectionMemberT OwnerT::*ptr>
//struct IteratorTraits<ptr> {
//    typedef OwnerT Owner;
//    typedef typename CollectionMemberT::iterator Iterator;
//
//};

template< typename OwnerT
        , typename KeyT, typename ValueT
        , std::unordered_map<KeyT, ValueT> OwnerT::*ptr>
struct IteratorTraits<ptr> {
    typedef OwnerT Owner;
    typedef typename std::unordered_map<KeyT, ValueT>::iterator Iterator;

    Iterator * create(Owner & owner, SelectionArgs &, Context & ctx) {
        // TODO: treat selector
        Iterator * itPtr = new Iterator;
        *itPtr = owner.*ptr.begin();
        return itPtr;
    }

    ValueT * dereference(Owner & owner, Iterator it, Context & ctx ) {
        assert(it != owner.end());
        return it->second;
    }

    Iterator * advance(Owner & owner, Iterator & it, Context & ctx) {
        ++it;
        return &it;
    }

    Iterator * reset(Owner & owner, Iterator & it, Context & ctx) {
        it = owner.begin();
    }

    void destroy(Owner & owner, Iterator & it, Context & ctx) {
        delete &it;
    }
};

/// Provides automatic implementations of iterator management functions for
/// a certain attribute. This is a thin shim aggregating all the potentially
/// unsafe reinterpret_cast's
template<auto T>
struct IteratorCastWrapper {
    static hdql_It_t
    create( hdql_Datum_t subj_
          , hdql_SelectionArgs_t selArgs
          , hdql_Context_t ctx_
          ) {
        assert(subj_);
        auto * subj = reinterpret_cast<typename IteratorTraits<T>::Owner*>(subj_);
        auto * ctx  = reinterpret_cast<hdql::Context*>(ctx_);
        return reinterpret_cast<hdql_It_t>(IteratorTraits<T>::create(*subj, selArgs, *ctx));
    }
    
    static hdql_Datum_t
    dereference( hdql_Datum_t subj_
               , hdql_It_t it_
               , hdql_Context_t ctx_
               ) {
        auto * subj = reinterpret_cast<typename IteratorTraits<T>::Owner*>(subj_);
        auto * ctx  = reinterpret_cast<hdql::Context*>(ctx_);
        auto * it   = reinterpret_cast<typename IteratorTraits<T>::Iterator*>(it_);
        return reinterpret_cast<hdql_Datum_t>(IteratorTraits<T>::dereference(*subj, *ctx));
    }

    static hdql_It_t
    advance( hdql_Datum_t subj_
           , hdql_It_t it_
           , hdql_Context_t ctx_
           ) {
        assert(subj_);
        assert(it_);
        auto * subj = reinterpret_cast<typename IteratorTraits<T>::Owner*>(subj_);
        auto * ctx  = reinterpret_cast<hdql::Context*>(ctx_);
        auto * it   = reinterpret_cast<typename IteratorTraits<T>::Iterator*>(it_);
        return reinterpret_cast<hdql_It_t>(IteratorTraits<T>::advance(*subj, *it, *ctx));
    }

    static hdql_It_t
    reset( hdql_Datum_t subj_
         , hdql_It_t it_
         , hdql_Context_t ctx_
         ) {
        assert(it_);
        auto * subj = reinterpret_cast<typename IteratorTraits<T>::Owner*>(subj_);
        auto * ctx  = reinterpret_cast<hdql::Context*>(ctx_);
        auto * it   = reinterpret_cast<typename IteratorTraits<T>::Iterator*>(it_);
        return IteratorTraits<T>::reset(*subj, *it, *ctx);
    }

    static void
    destroy( hdql_Datum_t subj_
           , hdql_It_t it_
           , hdql_Context_t ctx_
           ) {
        assert(it_);
        auto * subj = reinterpret_cast<typename IteratorTraits<T>::Owner*>(subj_);
        auto * ctx  = reinterpret_cast<hdql::Context*>(ctx_);
        auto * it   = reinterpret_cast<typename IteratorTraits<T>::Iterator*>(it_);
        IteratorTraits<T>::destroy(*subj, *it, *ctx);
    }
};

template<auto T> void
add_collection_attr( hdql_Compound & td
                   , const char * attrName
                   , hdql_AttrIdx_t attrIdx
                   , hdql_Compound * compoundDefinition
                   ) {
    hdql_AttributeDefinition attrDef;
    attrDef.isCollection = 0x1;
    attrDef.isAtomic = 0x0;
    attrDef.interface.collection.create       = IteratorCastWrapper<T>::create;
    attrDef.interface.collection.dereference  = IteratorCastWrapper<T>::dereference;
    attrDef.interface.collection.advance      = IteratorCastWrapper<T>::advance;
    attrDef.interface.collection.reset        = IteratorCastWrapper<T>::reset;
    attrDef.interface.collection.destroy      = IteratorCastWrapper<T>::destroy;
    attrDef.typeInfo.compound = compoundDefinition;
    hdql_compound_add_attr(&td, attrName, attrIdx, &attrDef);
}

template<auto T> void
add_collection_attr( hdql_Compound & td
                   , const char * attrName
                   , hdql_AttrIdx_t attrIdx
                   , const hdql_AtomicTypeFeatures & atomicAttrs
                   ) {
    hdql_AttributeDefinition attrDef;
    attrDef.isCollection = 0x1;
    attrDef.isAtomic = 0x0;
    attrDef.interface.collection.create       = IteratorCastWrapper<T>::create;
    attrDef.interface.collection.dereference  = IteratorCastWrapper<T>::dereference;
    attrDef.interface.collection.advance      = IteratorCastWrapper<T>::advance;
    attrDef.interface.collection.reset        = IteratorCastWrapper<T>::reset;
    attrDef.interface.collection.destroy      = IteratorCastWrapper<T>::destroy;
    attrDef.typeInfo.atomic = atomicAttrs;
}

}  // namespace hdql

