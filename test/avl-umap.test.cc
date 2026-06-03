#include <gtest/gtest.h>

#include "hdql/util/avl-tree.h"
#include "hdql/util/allocator.h"

class AVLULongKeyMap : public ::testing::Test {
protected:
    const hdql_Allocator * _allocator;
    hdql_umap *_m;
public:
    AVLULongKeyMap() : _allocator(&hdql_gHeapAllocator), _m(nullptr) {}
    void SetUp() override {
        _m = hdql_umap_create(_allocator);
    }

    void TearDown() override {
        if(_m) hdql_umap_destroy(_m);
    }
};

TEST_F(AVLULongKeyMap, emptyLookupReturnsNull) {
    EXPECT_EQ(nullptr, hdql_umap_get(_m, 42));
    EXPECT_EQ(hdql_umap_size(_m), 0);
}

TEST_F(AVLULongKeyMap, insertAndLookupSingleValue) {
    int value = 123;

    EXPECT_EQ(HDQL_AVL_OK, hdql_umap_insert(_m, 42, &value));
    EXPECT_EQ(&value, hdql_umap_get(_m, 42));
    EXPECT_EQ(hdql_umap_size(_m), 1);
}

TEST_F(AVLULongKeyMap, replaceExistingValue) {
    int v1 = 11;
    int v2 = 22;

    EXPECT_EQ(HDQL_AVL_OK,      hdql_umap_insert(_m, 42, &v1));
    EXPECT_EQ(HDQL_AVL_CHANGED, hdql_umap_insert(_m, 42, &v2));
    EXPECT_EQ(&v2, hdql_umap_get(_m, 42));
    EXPECT_EQ(hdql_umap_size(_m), 1);
}

TEST_F(AVLULongKeyMap, insertManyAndLookupAll) {
    std::vector<uint64_t> keys = {
        50, 20, 70, 10, 30, 60, 80, 25, 35, 65
    };

    std::vector<int> values(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = static_cast<int>(1000 + i);
        EXPECT_EQ(HDQL_AVL_OK, hdql_umap_insert(_m, keys[i], &values[i]));
        EXPECT_EQ(hdql_umap_size(_m), i+1);
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        EXPECT_EQ(&values[i], hdql_umap_get(_m, keys[i]));
    }

    EXPECT_EQ(nullptr, hdql_umap_get(_m, 999));
}

TEST_F(AVLULongKeyMap, eraseLeafNode) {
    int v10 = 10;
    int v20 = 20;
    int v30 = 30;

    auto k20 = 20;
    auto k10 = 10;
    auto k30 = 30;

    ASSERT_EQ(0, hdql_umap_insert(_m, k20, &v20));
    ASSERT_EQ(0, hdql_umap_insert(_m, k10, &v10));
    ASSERT_EQ(0, hdql_umap_insert(_m, k30, &v30));
    EXPECT_EQ(hdql_umap_size(_m), 3);

    void *old = nullptr;
    EXPECT_EQ(HDQL_AVL_CHANGED, hdql_umap_erase(_m, k10, &old));
    EXPECT_EQ(&v10, old);
    EXPECT_EQ(hdql_umap_size(_m), 2);

    EXPECT_EQ(nullptr, hdql_umap_get(_m, k10));
    EXPECT_EQ(&v20, hdql_umap_get(_m, k20));
    EXPECT_EQ(&v30, hdql_umap_get(_m, k30));
}

TEST_F(AVLULongKeyMap, eraseNodeWithTwoChildren) {
    std::vector<uint64_t> keys = {
        40, 20, 60, 10, 30, 50, 70
    };

    std::vector<int> values(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = static_cast<int>(keys[i]);
        ASSERT_EQ(0, hdql_umap_insert(_m, keys[i], &values[i]));
        EXPECT_EQ(hdql_umap_size(_m), i + 1);
    }

    void *old = nullptr;
    EXPECT_EQ(1, hdql_umap_erase(_m, 40, &old));
    EXPECT_EQ(&values[0], old);
    EXPECT_EQ(hdql_umap_size(_m), 6);

    EXPECT_EQ(nullptr, hdql_umap_get(_m, 40));

    for (size_t i = 1; i < keys.size(); ++i) {
        EXPECT_EQ(&values[i], hdql_umap_get(_m, keys[i]));
    }
}

TEST_F(AVLULongKeyMap, eraseMissingKeyReturnsZero) {
    int value = 123;

    ASSERT_EQ(0, hdql_umap_insert(_m, 1, &value));
    EXPECT_EQ(hdql_umap_size(_m), 1);

    void *old = reinterpret_cast<void *>(0xdeadbeef);
    EXPECT_EQ(0, hdql_umap_erase(_m, 2, &old));

    /* old should remain untouched if key was absent */
    EXPECT_EQ(reinterpret_cast<void *>(0xdeadbeef), old);
    EXPECT_EQ(&value, hdql_umap_get(_m, 1));
    EXPECT_EQ(hdql_umap_size(_m), 1);
}

/*
 * AVL iteration
 */

struct IterState {
    std::vector<uint64_t> keys;
    std::vector<void *> values;
};

static int collect_iter_cb(
        const unsigned long int key,
        void **value,
        void *userdata)
{
    auto *st = static_cast<IterState *>(userdata);

    st->keys.push_back(key);
    st->values.push_back(*value);

    return 0;
}

TEST_F(AVLULongKeyMap, IteratesInSortedOrder) {
    std::vector<uint64_t> keys = {
        5, 1, 9, 3, 7, 2, 8
    };

    std::vector<int> values(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = static_cast<int>(keys[i]);
        ASSERT_EQ(0, hdql_umap_insert(_m, keys[i], &values[i]));
    }

    IterState st;
    EXPECT_EQ(0, hdql_umap_iter(_m, collect_iter_cb, &st));

    std::vector<uint64_t> expected = keys;
    std::sort(expected.begin(), expected.end());

    EXPECT_EQ(expected, st.keys);
}

struct StopState {
    int count = 0;
};

static int stop_after_three_cb(
        const unsigned long key,
        void **value,
        void *userdata)
{
    (void) key;
    (void) value;

    auto *st = static_cast<StopState *>(userdata);

    ++st->count;

    if (st->count == 3)
        return 1234;

    return 0;
}

TEST_F(AVLULongKeyMap, iterationCanStopEarly) {
    int values[10];

    for (uint64_t i = 0; i < 10; ++i) {
        values[i] = static_cast<int>(i);
        ASSERT_EQ(0, hdql_umap_insert(_m, i, &values[i]));
    }

    StopState st;

    EXPECT_EQ(1234, hdql_umap_iter(_m, stop_after_three_cb, &st));
    EXPECT_EQ(3, st.count);
}

struct ReplaceState {
    int replacement = 777;
};

static int replace_even_keys_cb(
        const unsigned long key,
        void **value,
        void *userdata)
{
    auto *st = static_cast<ReplaceState *>(userdata);

    if ((key % 2) == 0)
        *value = &st->replacement;

    return 0;
}

TEST_F(AVLULongKeyMap, iterationCallbackMayReplaceValues) {
    int values[6];

    for (uint64_t i = 0; i < 6; ++i) {
        values[i] = static_cast<int>(i);
        ASSERT_EQ(0, hdql_umap_insert(_m, i, &values[i]));
    }

    ReplaceState st;

    EXPECT_EQ(0, hdql_umap_iter(_m, replace_even_keys_cb, &st));

    for (uint64_t i = 0; i < 6; ++i) {
        if ((i % 2) == 0)
            EXPECT_EQ(&st.replacement, hdql_umap_get(_m, i));
        else
            EXPECT_EQ(&values[i], hdql_umap_get(_m, i));
    }
}

