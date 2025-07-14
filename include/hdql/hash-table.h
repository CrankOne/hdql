#ifndef HDQL_HASH_TABLE_H
#define HDQL_HASH_TABLE_H

/*\brief HDQL internal hash table implementation
 *
 * \todo Currently STL containers outperforms this implementation till <30
 *       elements on records. Try to use linear scan/bijection on sorted array
 *       for some reasonably small number of elements (<8 for LS, <30 for
 *       sorted) instead of hash table, switch to HT when number of entries
 *       becomes larger than 30.
 */

#include "hdql/allocator.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HDQL_MURMUR3_32_DEFAULT_SEED 0x9747b28c

#define HDQL_HT_RC_OK           0
#define HDQL_HT_RC_UPDATED      1
#define HDQL_HT_RC_INSERTED     2
#define HDQL_HT_ERROR           (0x1 << 7)
#define HDQL_HT_ERR_MEM         (HDQL_HT_ERROR | 1)
#define HDQL_HT_RC_ERR_NOENT    (HDQL_HT_ERROR | 2)

typedef struct hdql_ht hdql_ht;  /* fwd, opaque */
struct hdql_htEntry;  /* fwd, opaque */

/**\brief Creates a new hash table with starting capacity
 *
 * Initial capacity affects container performance and memory consumption.
 * Generally, set x2-x4 times greater to gain best
 * performance/memory consumption ratio.
 *
 * \returns NULL on memory allocation error.
 * */
hdql_ht * hdql_ht_create(const struct hdql_Allocator *, size_t dftCapacity, size_t hashSeed );

/**\brief Destroys the hash table
 *
 * Instance must be previously created with `hdql_ht_create()` */
void hdql_ht_destroy(hdql_ht *ht);

/**\brief Inserts or updates a key-value pair
 *
 * \returns `HDQL_HT_RC_INSERTED` if new element was added
 * \returns `HDQL_HT_RC_UPDATED` if existing entry was overwritten.
 * \returns `HDQL_HT_ERR_MEM` on memory allocation failure for new element
 * \returns forwarded results from `hdql_ht_rebuild()` on capacity change failures
 * */
int hdql_ht_ins( hdql_ht * ht
            , const unsigned char * key, size_t keyLen
            , void * value);

/** Finds entry in hash table
 *
 * Returned item is an opaque structure used as a hint to enhance
 * find/update/erase procedures. \p nb is internal parameter (entry bucket
 * number) used as aux input for some of these procedures.
 * */
struct hdql_htEntry *
hdql_ht_lookup( const hdql_ht * ht
        , const unsigned char * key, size_t keyLen
        , size_t * nb
        );

/** Returns pointer to value or NULL if not found */
void * hdql_ht_get(const hdql_ht * ht, const unsigned char * key, size_t keyLen);

int hdql_ht_erase( hdql_ht * ht, struct hdql_htEntry * entry, size_t nb );

/** Removes key from hash table; returns 1 if removed, 0 if not found */
int hdql_ht_remove(hdql_ht *ht, const unsigned char *key, size_t keyLen);

/**\brief Iterates through all key-value pairs
 *
 * Given callback may return non-zero to interrupt iteration (result will
 * be forwarded to this functions return code). The value is given by pointer,
 * so callback function may alterate its content, if needed.
 *
 * Returns 0 when done.
 * */
int hdql_ht_iter( const hdql_ht * ht
        , int (*callback)(const unsigned char * key, size_t keyLen, void ** value, void * userdata)
        , void * userdata
        );

/**\brief Reallocates storage to support x2 items, recalculates hashes */
int hdql_ht_rebuild(hdql_ht *ht);


/* Wrappers for string keys
 */

/**\brief Inserts or updates a key-value pair with string key
 *
 * Wrapper around `hdql_ht_ins()` for C strings, same semantics
 * */
int hdql_ht_s_ins( hdql_ht * ht, const char * key, void * value);

/**\brief Wrapper for `hdql_ht_get()` for C-string key */
void * hdql_ht_s_get(const hdql_ht * ht, const char * key);

/**\brief Wrapper for `hdql_ht_remove()` for C-string key */
int hdql_ht_s_rm(hdql_ht * ht, const char * key);

#ifdef __cplusplus
}
#endif

#endif // HDQL_HASH_TABLE_H

