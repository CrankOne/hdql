#include "hdql/hash-table.h"

#include <stdint.h>
#include <assert.h>
#include <string.h>

#include <stdio.h>  // XXX

typedef struct hdql_htEntry {
    /* Item key, as is */
    uint8_t * key;
    /* Key length */
    size_t keySize;
    /* Stored value */
    void * value;
    /* Ptr to next element in a bucket or NULL */
    struct hdql_htEntry * next;
} hdql_ht_entry;

struct hdql_ht {
    /* Array of buckets (linked lists) referring to items with
     * hash%capacity == N */
    hdql_ht_entry ** buckets;
    size_t size  /* Number of records (pairs) stored */
         , capacity  /* number of buckets in use */
         , mask  /* bitmask for faster modulo operation */
         ;  
    struct hdql_Allocator allocator;
    size_t hashSeed;
};

#if 0
// A little slower than murmur (!)
static uint32_t djb2_hash(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + (uint32_t)c;
    return hash;
}

#define murmur3_32(s, l, ss) djb2_hash((const char*) s)
#else

/*
 * Murmur32 hash func assets
 */
static uint32_t murmur3_32(const uint8_t *key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const int nblocks = len >> 2;
    const uint32_t *blocks = (const uint32_t *)(key);
    for (int i = 0; i < nblocks; ++i) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << 15) | (k >> 17);
        k *= c2;

        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    const uint8_t *tail = (const uint8_t *)(key + (nblocks << 2));
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;  // fallthrough
        case 2: k1 ^= tail[1] << 8;   // fallthrough
        case 1: k1 ^= tail[0];
                k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2;
                h ^= k1;
    }

    h ^= len;
    h ^= (h >> 16);
    h *= 0x85ebca6b;
    h ^= (h >> 13);
    h *= 0xc2b2ae35;
    h ^= (h >> 16);

    return h;
}
#endif

hdql_ht *
hdql_ht_create(const struct hdql_Allocator * a, size_t dftCap, size_t hashSeed) {
    hdql_ht * ht = (hdql_ht *) a->alloc(sizeof(hdql_ht), a->userdata);
    if (!ht) return NULL;  /* Error: can't allocate ht instance */
    ht->buckets = (hdql_ht_entry **) a->alloc(
                  (1u << dftCap) * sizeof(hdql_ht_entry *)
                , a->userdata
                );
    if (!ht->buckets) {
        a->free(ht, a->userdata);
        return NULL;  /* Error: can't allocate buckets for HT */
    }
    memset( ht->buckets, 0x0, (1u << dftCap) * sizeof(hdql_ht_entry *) );
    ht->size = 0;
    ht->capacity = dftCap;
    ht->mask = (1u << ht->capacity) - 1;
    ht->allocator = *a;
    ht->hashSeed = hashSeed;
    return ht;
}

struct hdql_htEntry *
hdql_ht_lookup( const hdql_ht * ht
        , const unsigned char * key
        , size_t keyLen
        , size_t * nb
        ) {
    assert(nb);
    const uint32_t h  = murmur3_32(key, keyLen, ht->hashSeed);
    //*nb = h % (1u << ht->capacity);
    const size_t nbl = *nb = h & ht->mask;
    hdql_ht_entry * entry = ht->buckets[nbl];
    while (entry) {
        if( entry->keySize != keyLen
         || 0 != memcmp(entry->key, key, keyLen)) {
            entry = entry->next;
            continue;
        }
        return entry;
    }
    return NULL;
}

int
hdql_ht_ins( hdql_ht * ht
           , const unsigned char * key, size_t keyLen
           , void * value
           ) {
    int rc;
    if( ht->size > (1u << (ht->capacity-1))) {
        rc = hdql_ht_rebuild(ht);  /* extend capacity */
        if(HDQL_HT_ERROR & rc) {
            return rc;  /* Error: failed to rebuild */
        }
    }
    size_t nb;
    hdql_ht_entry * entry = hdql_ht_lookup(ht, key, keyLen, &nb);
    if(NULL != entry) {
        assert(entry->keySize == keyLen);
        assert(0 == memcmp(key, entry->key, keyLen));
        entry->value = value;
        return HDQL_HT_RC_UPDATED;  /* updated */
    }
    /* otherwise, insert new entry */
    unsigned char * keyCopy
        = (unsigned char *) ht->allocator.alloc(keyLen, ht->allocator.userdata);
    if(!keyCopy) {
        return HDQL_HT_ERR_MEM;  /* Error: failed to allocate key copy */
    }
    memcpy(keyCopy, key, keyLen);

    entry = (hdql_ht_entry *) ht->allocator.alloc(sizeof(hdql_ht_entry)
            , ht->allocator.userdata);
    if(!entry) {
        ht->allocator.free(keyCopy, ht->allocator.userdata);
        return -3;  /* Error: failed to allocate new entry */
    }

    entry->key = keyCopy;
    entry->keySize = keyLen;
    entry->value = value;
    entry->next = ht->buckets[nb];
    ht->buckets[nb] = entry;
    ++ht->size;
    return HDQL_HT_RC_INSERTED;  /* inserted new item */
}

void *
hdql_ht_get(const hdql_ht *ht, const unsigned char * key, size_t keyLen) {
    /* Benchmarks show visible constant slowdown because of additional
     * arguments and redirections for few (1-30) entries. */
    #if 1
    size_t nb;
    hdql_ht_entry * entry = hdql_ht_lookup(ht, key, keyLen, &nb);
    if(entry) return entry->value;
    return NULL;
    #else
    const uint32_t h  = murmur3_32((const unsigned char *) key, keyLen, ht->hashSeed);
    const uint32_t nbl = h % (1u << ht->capacity);
    //const size_t nbl = h & ht->mask;
    hdql_ht_entry * entry = ht->buckets[nbl];
    while (entry) {
        if( entry->keySize != keyLen
         || 0 != memcmp(entry->key, key, keyLen)) {
            entry = entry->next;
        }
        return entry;
    }
    return NULL;
    #endif
}

int
hdql_ht_remove( hdql_ht * ht, const unsigned char * key, size_t keyLen ) {
    size_t nb;
    hdql_ht_entry * entry = hdql_ht_lookup(ht, key, keyLen, &nb);
    if(NULL == entry) return HDQL_HT_RC_ERR_NOENT;
    return hdql_ht_erase(ht, entry, nb);
}

int
hdql_ht_erase( hdql_ht * ht, hdql_ht_entry * entry, size_t nb ) {
    assert(entry);
    assert(nb < (1u << ht->capacity));
    hdql_ht_entry  * cur   = ht->buckets[nb]
                , ** prevP = ht->buckets + nb;
    while( cur ) {
        if( cur == entry ) {
            *prevP = cur->next;
            ht->allocator.free(entry->key, ht->allocator.userdata);
            ht->allocator.free(entry,      ht->allocator.userdata);
            return HDQL_HT_RC_OK;
        }
        /* match found */
        prevP = &cur->next;
        cur   = cur->next;
    }
    return HDQL_HT_RC_ERR_NOENT;
}

int
hdql_ht_iter( const hdql_ht * ht
        , int (* callback)(const unsigned char * key, size_t keyLen, void ** value, void * userdata)
        , void * userdata
        ) {
    const size_t cap = (1u << ht->capacity);
    for (size_t i = 0; i < cap; ++i) {
        hdql_ht_entry * entry = ht->buckets[i];
        while (entry) {
            int rc = callback(entry->key, entry->keySize, &entry->value, userdata);
            if(rc != 0) return rc;
            entry = entry->next;
        }
    }
    return 0;
}

int
hdql_ht_rebuild( hdql_ht * ht ) {
    size_t newCap = ht->capacity ? ht->capacity + 1 : 5;
    hdql_ht_entry ** newBuckets =
        (hdql_ht_entry **) ht->allocator.alloc(
                (1u << newCap) * sizeof(void *), ht->allocator.userdata);
    if(NULL == newBuckets)
        return HDQL_HT_ERR_MEM;
    memset(newBuckets, 0x0, (1u << newCap) * sizeof(void *));

    const size_t cap = (1u << ht->capacity);
    for( size_t i = 0; i < cap; ++i ) {
        hdql_ht_entry * entry = ht->buckets[i];
        while( entry ) {
            hdql_ht_entry * next = entry->next;

            const uint32_t h  = murmur3_32(entry->key, entry->keySize, ht->hashSeed);
            const size_t newNB = /* h % newCap*/ h & ((1u << newCap) - 1);

            entry->next = newBuckets[newNB];
            newBuckets[newNB] = entry;

            entry = next;
        }
    }

    ht->allocator.free(ht->buckets, ht->allocator.userdata);
    ht->buckets = newBuckets;
    ht->capacity = newCap;
    ht->mask = ((1u << newCap) - 1);
    return HDQL_HT_RC_OK;
}

void hdql_ht_destroy(hdql_ht * ht) {
    const size_t cap = (1u << ht->capacity);
    /* iterate over all entries and free key */
    for(size_t i = 0; i < cap; ++i) {
        hdql_ht_entry * entry = ht->buckets[i];
        while(entry) {
            ht->allocator.free(entry->key, ht->allocator.userdata);
            hdql_ht_entry * next = entry->next;
            ht->allocator.free(entry, ht->allocator.userdata);
            entry = next;
        }
    }
    /* free buckets and object */
    ht->allocator.free(ht->buckets, ht->allocator.userdata);
    ht->allocator.free(ht,          ht->allocator.userdata);
}

/*
 * String wrappers
 */

int
hdql_ht_s_ins( hdql_ht * ht
            , const char * key
            , void * value ) {
    return hdql_ht_ins(ht, (unsigned char *) key, strlen(key), value);
}

void *
hdql_ht_s_get(const hdql_ht * ht, const char * key) {
    /* Benchmarks show visible constant slowdown because of additional
     * arguments and redirections for few (1-30) entries. */
    #if 1
    return hdql_ht_get(ht, (unsigned char *) key, strlen(key) );
    #else
    const uint32_t keyLen = strlen(key);
    const uint32_t h  = murmur3_32((const unsigned char *) key, keyLen, ht->hashSeed);
    const uint32_t nbl = h % (1u << ht->capacity);
    //const size_t nbl = h & ht->mask;
    hdql_ht_entry * entry = ht->buckets[nbl];
    while (entry) {
        if( entry->keySize != keyLen
         || 0 != memcmp(entry->key, key, keyLen)) {
            entry = entry->next;
        }
        return entry;
    }
    return NULL;
    #endif
}

int
hdql_ht_s_rm(hdql_ht * ht, const char * key) {
    return hdql_ht_remove(ht, (const unsigned char *) key, strlen(key) );
}

