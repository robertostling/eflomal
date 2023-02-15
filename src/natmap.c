/* natmap.c: efficient maps between native data types.
 *
 * There are two main implementations under the hood: sorted lookup table for
 * small maps (small enough that linear rather than binary search is used),
 * and hash tables for larger maps.
 *
 * Configuration is done by (re)defining the macros below and including this
 * file. See test.c or efmugal.c for examples of how to do this in practice.
 *
 * MAKE_NAME(NAME)                  macro to create local identifiers
 * MAX_FIXED                        maximum numbers of elements when using
 *                                  fixed-size (non-hash) table, should be
 *                                  2^n-1 for some integer n
 * INDEX_TYPE                       type of table indexes (e.g. size_t)
 * KEY_TYPE                         type of keys (e.g. uint32_t)
 * VALUE_TYPE                       type of values
 * EMPTY_KEY                        value of key for empty slots
 * INDEX_TYPE HASH_KEY(KEY_TYPE)    function to hash KEY_TYPE into INDEX_TYPE
 */

#include <stdlib.h>
#include <string.h>

#if (MAX_FIXED&(MAX_FIXED+1))
#error "MAX_FIXED must be one less than a power of 2"
#endif

#define STRUCT_NAME         MAKE_NAME()
#define FUN_CREATE          MAKE_NAME(_create)
#define FUN_RESET           MAKE_NAME(_reset)
#define FUN_CLEAR           MAKE_NAME(_clear)
#define FUN_INSERT          MAKE_NAME(_insert)
#define FUN_DELETE          MAKE_NAME(_delete)
#define FUN_GET             MAKE_NAME(_get)
#define FUN_GET_PTR         MAKE_NAME(_get_ptr)
#define FUN_ADD             MAKE_NAME(_add)
#define FUN_ITEMS           MAKE_NAME(_items)

#define FUN_LOOKUP_FIXED    MAKE_NAME(_lookup_fixed)
#define FUN_INSERT_FIXED    MAKE_NAME(_insert_fixed)
#define FUN_DELETE_FIXED    MAKE_NAME(_delete_fixed)
#define FUN_LOOKUP_DYNAMIC  MAKE_NAME(_lookup_dynamic)
#define FUN_INSERT_DYNAMIC  MAKE_NAME(_insert_dynamic)
#define FUN_DELETE_DYNAMIC  MAKE_NAME(_delete_dynamic)
#define FUN_RESIZE_DYNAMIC  MAKE_NAME(_resize_dynamic)
#define FUN_MAKE_DYNAMIC    MAKE_NAME(_make_dynamic)
#define FUN_IS_DYNAMIC      MAKE_NAME(_is_dynamic)

// Minimun size of hash table if MAX_FIXED is 0
//
//  Timings
// 4  : 22.97
// 8  : 22.72
// 16 : 22.60
#define MIN_DYNAMIC         4

// This saves some memory, but the time difference is negligible.
#define MERGE_MALLOC

struct STRUCT_NAME {
    INDEX_TYPE n_items;
#if MAX_FIXED != 0
    unsigned int dynamic : 1;
#endif
    union {
        struct {
            KEY_TYPE keys[MAX_FIXED];
            VALUE_TYPE values[MAX_FIXED];
        } fixed;
        struct {
            INDEX_TYPE size;
            KEY_TYPE *keys;
            VALUE_TYPE *values;
        } dynamic;
    } structure;
};

static int FUN_RESIZE_DYNAMIC(struct STRUCT_NAME *m, INDEX_TYPE new_size);

// TODO: use one malloc() call instead of two when allocating dynamic
// keys/values arrays.
// TODO: add FUN_GET_HASH() which also takes a precomputed key hash

static inline int FUN_IS_DYNAMIC(const struct STRUCT_NAME *m) {
#if MAX_FIXED == 0
    return 1;
#else
    return m->dynamic;
#endif
}

// Free any buffers and put the map into the same state as after FUN_CREATE()
static void FUN_CLEAR(struct STRUCT_NAME *m) {
    if (FUN_IS_DYNAMIC(m)) {
        free(m->structure.dynamic.keys);
#ifndef MERGE_MALLOC
        free(m->structure.dynamic.values);
#endif
    }
    m->n_items = 0;
#if MAX_FIXED != 0
    m->dynamic = 0;
#endif
}

static int FUN_LOOKUP_FIXED(
        const struct STRUCT_NAME *m, KEY_TYPE key, INDEX_TYPE *index)
{
    for (INDEX_TYPE i=0; i<m->n_items; i++) {
        const KEY_TYPE k = m->structure.fixed.keys[i];
        if (k >= key) {
            *index = i;
            return k == key;
        }
    }
    *index = m->n_items;
    return 0;
}

static void FUN_ITEMS(
        struct STRUCT_NAME *m, KEY_TYPE *key_buf, VALUE_TYPE *value_buf)
{
    if (FUN_IS_DYNAMIC(m)) {
        const KEY_TYPE *keys = m->structure.dynamic.keys;
        const VALUE_TYPE *values = m->structure.dynamic.values;
        size_t n = 0;
        for (size_t i=0; i<m->structure.dynamic.size; i++) {
            if (keys[i] != EMPTY_KEY) {
                key_buf[n] = keys[i];
                value_buf[n] = values[i];
                n++;
            }
        }
        assert(n == m->n_items);
    } else {
        memcpy(key_buf, m->structure.fixed.keys,
               m->n_items*sizeof(KEY_TYPE));
        memcpy(value_buf, m->structure.fixed.values,
               m->n_items*sizeof(VALUE_TYPE));
    }
}

static void FUN_INSERT_FIXED(
        struct STRUCT_NAME *m, INDEX_TYPE index,
        KEY_TYPE key, VALUE_TYPE value)
{
    KEY_TYPE *keys = m->structure.fixed.keys;
    KEY_TYPE *values = m->structure.fixed.values;
    if (index < m->n_items && keys[index] == key) {
        values[index] = value;
        return;
    }
    for (INDEX_TYPE i=index; i<m->n_items; i++) {
        const KEY_TYPE next_key = keys[i];
        const VALUE_TYPE next_value = values[i];
        keys[i] = key;
        values[i] = value;
        key = next_key;
        value = next_value;
    }
    keys[m->n_items] = key;
    values[m->n_items] = value;
    m->n_items++;
}

static void FUN_DELETE_FIXED(struct STRUCT_NAME *m, INDEX_TYPE index) {
    KEY_TYPE *keys = m->structure.fixed.keys;
    KEY_TYPE *values = m->structure.fixed.values;
    for (INDEX_TYPE i=index; i<m->n_items-1; i++) {
        keys[i] = keys[i+1];
        values[i] = values[i+1];
    }
    m->n_items--;
}

static int FUN_LOOKUP_DYNAMIC(
        const struct STRUCT_NAME *m, KEY_TYPE key, INDEX_TYPE *index,
        INDEX_TYPE key_hash)
{
    const INDEX_TYPE mask = m->structure.dynamic.size - 1;
    INDEX_TYPE i = key_hash & mask;
    const KEY_TYPE *keys = m->structure.dynamic.keys;
    while(1) {
        if (keys[i] == key) {
            *index = i;
            return 1;
        } else if (keys[i] == EMPTY_KEY) {
            *index = i;
            return 0;
        }
        i = (i+1) & mask;
    }
}

static int FUN_INSERT_DYNAMIC(
        struct STRUCT_NAME *m, KEY_TYPE key, VALUE_TYPE value,
        INDEX_TYPE key_hash)
{
    if (m->n_items*2 > m->structure.dynamic.size)
        FUN_RESIZE_DYNAMIC(m, m->structure.dynamic.size*2);

    INDEX_TYPE index;
    KEY_TYPE *keys = m->structure.dynamic.keys;
    KEY_TYPE *values = m->structure.dynamic.values;
    if (FUN_LOOKUP_DYNAMIC(m, key, &index, key_hash)) {
        values[index] = value;
        return 1;
    }
    values[index] = value;
    keys[index] = key;
    m->n_items++;
    return 0;
}

static void FUN_DELETE_DYNAMIC(struct STRUCT_NAME *m, INDEX_TYPE index) {
    m->n_items--;
    INDEX_TYPE i,j,k;
    const INDEX_TYPE mask = m->structure.dynamic.size - 1;
    KEY_TYPE *keys = m->structure.dynamic.keys;
    KEY_TYPE *values = m->structure.dynamic.values;
    i = j = index;
    while(1) {
        keys[i] = EMPTY_KEY;
        while(1) {
            j = (j+1) & mask;
            if (keys[j] == EMPTY_KEY) return;
            k = HASH_KEY(keys[j]) & mask;
            if(!((i<=j)? ((i<k) && (k<=j)): ((i<k) || (k<=j)))) break;
        }
        keys[i] = keys[j];
        values[i] = values[j];
        i = j;
    }
}

static int FUN_RESIZE_DYNAMIC(struct STRUCT_NAME *m, INDEX_TYPE new_size) {
    KEY_TYPE *old_keys = m->structure.dynamic.keys;
    VALUE_TYPE *old_values = m->structure.dynamic.values;
    const INDEX_TYPE old_size = m->structure.dynamic.size;
#ifdef MERGE_MALLOC
    if ((m->structure.dynamic.keys =
                malloc(new_size*(sizeof(KEY_TYPE) + sizeof(VALUE_TYPE))))
            == NULL)
    {
        perror("FUN_RESIZE_DYNAMIC(): unable to allocate arrays");
        exit(EXIT_FAILURE);
    }
    m->structure.dynamic.values = (VALUE_TYPE*)(
            m->structure.dynamic.keys + new_size);
#else
    if ((m->structure.dynamic.keys = malloc(new_size*sizeof(KEY_TYPE)))
            == NULL) {
        perror("FUN_RESIZE_DYNAMIC(): unable to allocate keys array");
        exit(EXIT_FAILURE);
    }
    if ((m->structure.dynamic.values = malloc(new_size*sizeof(VALUE_TYPE)))
            == NULL) {
        perror("FUN_RESIZE_DYNAMIC(): unable to allocate values array");
        exit(EXIT_FAILURE);
    }
#endif
    m->n_items = 0;
    m->structure.dynamic.size = new_size;
    for (INDEX_TYPE i=0; i<new_size; i++)
        m->structure.dynamic.keys[i] = EMPTY_KEY;
    for (INDEX_TYPE i=0; i<old_size; i++) {
        if (old_keys[i] != EMPTY_KEY) {
            FUN_INSERT_DYNAMIC(m, old_keys[i], old_values[i],
                               HASH_KEY(old_keys[i]));
        }
    }
    free(old_keys);
#ifndef MERGE_MALLOC
    free(old_values);
#endif
    return 0;
}

static int FUN_MAKE_DYNAMIC(struct STRUCT_NAME *m, INDEX_TYPE new_size) {
#if MAX_FIXED != 0
    KEY_TYPE old_keys[MAX_FIXED];
    VALUE_TYPE old_values[MAX_FIXED];
    memcpy(old_keys, m->structure.fixed.keys, m->n_items*sizeof(KEY_TYPE));
    memcpy(old_values, m->structure.fixed.values,
           m->n_items*sizeof(VALUE_TYPE));
    INDEX_TYPE old_n_items = m->n_items;
#endif

#ifdef MERGE_MALLOC
    if ((m->structure.dynamic.keys =
                malloc(new_size*(sizeof(KEY_TYPE) + sizeof(VALUE_TYPE))))
            == NULL)
    {
        perror("FUN_MAKE_DYNAMIC(): unable to allocate arrays");
        exit(EXIT_FAILURE);
    }
    m->structure.dynamic.values = (VALUE_TYPE*)(
            m->structure.dynamic.keys + new_size);
#else
    if ((m->structure.dynamic.keys = malloc(new_size*sizeof(KEY_TYPE)))
            == NULL) {
        perror("FUN_MAKE_DYNAMIC(): unable to allocate keys array");
        exit(EXIT_FAILURE);
    }
    if ((m->structure.dynamic.values = malloc(new_size*sizeof(VALUE_TYPE)))
            == NULL) {
        perror("FUN_MAKE_DYNAMIC(): unable to allocate values array");
        exit(EXIT_FAILURE);
    }
#endif
#if MAX_FIXED != 0
    m->dynamic = 1;
#endif
    m->n_items = 0;
    m->structure.dynamic.size = new_size;
    for (INDEX_TYPE i=0; i<new_size; i++)
        m->structure.dynamic.keys[i] = EMPTY_KEY;
#if MAX_FIXED != 0
    for (INDEX_TYPE i=0; i<old_n_items; i++) {
        FUN_INSERT_DYNAMIC(m, old_keys[i], old_values[i],
                           HASH_KEY(old_keys[i]));
    }
#endif
    return 0;
}

static int FUN_INSERT(struct STRUCT_NAME *m, KEY_TYPE key, VALUE_TYPE value) {
#if MAX_FIXED != 0
    if (FUN_IS_DYNAMIC(m) == 0 && m->n_items == MAX_FIXED)
        FUN_MAKE_DYNAMIC(m, (MAX_FIXED+1)*4);
#endif

    if (FUN_IS_DYNAMIC(m)) {
        return FUN_INSERT_DYNAMIC(m, key, value, HASH_KEY(key));
    } else {
        INDEX_TYPE index;
        const int r = FUN_LOOKUP_FIXED(m, key, &index);
        FUN_INSERT_FIXED(m, index, key, value);
        return r;
    }
}

static int FUN_DELETE(struct STRUCT_NAME *m, KEY_TYPE key) {
    if (FUN_IS_DYNAMIC(m)) {
        INDEX_TYPE index;
        if (FUN_LOOKUP_DYNAMIC(m, key, &index, HASH_KEY(key))) {
            FUN_DELETE_DYNAMIC(m, index);
            return 1;
        }
    } else {
        INDEX_TYPE index;
        if (FUN_LOOKUP_FIXED(m, key, &index)) {
            FUN_DELETE_FIXED(m, index);
            return 1;
        }
    }
    return 0;
}

static VALUE_TYPE *FUN_GET_PTR(struct STRUCT_NAME *m, KEY_TYPE key) {
    if (FUN_IS_DYNAMIC(m)) {
        INDEX_TYPE index;
        if (FUN_LOOKUP_DYNAMIC(m, key, &index, HASH_KEY(key)))
            return m->structure.dynamic.values + index;
    } else {
        INDEX_TYPE index;
        if (FUN_LOOKUP_FIXED(m, key, &index)) {
            return m->structure.fixed.values + index;
        }
    }
    return NULL;
}

static int FUN_GET(struct STRUCT_NAME *m, KEY_TYPE key, VALUE_TYPE *value) {
    VALUE_TYPE *ptr = FUN_GET_PTR(m, key);
    if (ptr == NULL) return 0;
    *value = *ptr;
    return 1;
}

static VALUE_TYPE FUN_ADD(
        struct STRUCT_NAME *m, KEY_TYPE key, VALUE_TYPE value)
{
    VALUE_TYPE *ptr = FUN_GET_PTR(m, key);
    if (ptr == NULL) {
        FUN_INSERT(m, key, value);
        return value;
    } else {
        (*ptr) += value;
        return *ptr;
    }
}

static void FUN_CREATE(struct STRUCT_NAME *m) {
    m->n_items = (INDEX_TYPE)0;
#if MAX_FIXED == 0
    FUN_MAKE_DYNAMIC(m, MIN_DYNAMIC);
#else
    m->dynamic = 0;
    for (INDEX_TYPE i=0; i<MAX_FIXED; i++)
        m->structure.fixed.keys[i] = EMPTY_KEY;
#endif
}

static void FUN_RESET(struct STRUCT_NAME *m) {
    if (FUN_IS_DYNAMIC(m)) {
        FUN_CLEAR(m);
#if MAX_FIXED == 0
        FUN_CREATE(m);
#endif
    } else {
        m->n_items = 0;
    }
}

