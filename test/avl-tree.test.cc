#include <gtest/gtest.h>

#include "hdql/util/avl-tree.h"
#include "hdql/util/allocator.h"

class TestAVLTree : public ::testing::Test {
protected:
    const hdql_Allocator * _allocator;
    hdql_fmap *_m;
public:
    TestAVLTree() : _allocator(&hdql_gHeapAllocator), _m(nullptr) {}
    void SetUp() override {
        _m = hdql_fmap_create(8, _allocator);
    }

    void TearDown() override {
        if(_m) hdql_fmap_destroy(_m);
    }
};

static void u64_to_be(uint64_t x, unsigned char out[8]) {
    for (int i = 7; i >= 0; --i) {
        out[i] = static_cast<unsigned char>(x & 0xffu);
        x >>= 8;
    }
}

static std::vector<unsigned char> key_u64(uint64_t x) {
    std::vector<unsigned char> k(8);
    u64_to_be(x, k.data());
    return k;
}

TEST_F(TestAVLTree, emptyLookupReturnsNull) {
    auto k = key_u64(42);

    EXPECT_EQ(nullptr, hdql_fmap_get(_m, k.data()));
    EXPECT_EQ(hdql_fmap_size(_m), 0);
}

TEST_F(TestAVLTree, insertAndLookupSingleValue) {
    int value = 123;
    auto k = key_u64(42);

    EXPECT_EQ(HDQL_AVL_OK, hdql_fmap_insert(_m, k.data(), &value));
    EXPECT_EQ(&value, hdql_fmap_get(_m, k.data()));
    EXPECT_EQ(hdql_fmap_size(_m), 1);
}

TEST_F(TestAVLTree, replaceExistingValue) {
    int v1 = 11;
    int v2 = 22;
    auto k = key_u64(42);

    EXPECT_EQ(HDQL_AVL_OK,      hdql_fmap_insert(_m, k.data(), &v1));
    EXPECT_EQ(HDQL_AVL_CHANGED, hdql_fmap_insert(_m, k.data(), &v2));
    EXPECT_EQ(&v2, hdql_fmap_get(_m, k.data()));
    EXPECT_EQ(hdql_fmap_size(_m), 1);
}

TEST_F(TestAVLTree, insertManyAndLookupAll) {
    std::vector<uint64_t> keys = {
        50, 20, 70, 10, 30, 60, 80, 25, 35, 65
    };

    std::vector<int> values(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = static_cast<int>(1000 + i);
        auto k = key_u64(keys[i]);
        EXPECT_EQ(HDQL_AVL_OK, hdql_fmap_insert(_m, k.data(), &values[i]));
        EXPECT_EQ(hdql_fmap_size(_m), i+1);
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        auto k = key_u64(keys[i]);
        EXPECT_EQ(&values[i], hdql_fmap_get(_m, k.data()));
    }

    auto missing = key_u64(999);
    EXPECT_EQ(nullptr, hdql_fmap_get(_m, missing.data()));
}

TEST_F(TestAVLTree, eraseLeafNode) {
    int v10 = 10;
    int v20 = 20;
    int v30 = 30;

    auto k20 = key_u64(20);
    auto k10 = key_u64(10);
    auto k30 = key_u64(30);

    ASSERT_EQ(0, hdql_fmap_insert(_m, k20.data(), &v20));
    ASSERT_EQ(0, hdql_fmap_insert(_m, k10.data(), &v10));
    ASSERT_EQ(0, hdql_fmap_insert(_m, k30.data(), &v30));
    EXPECT_EQ(hdql_fmap_size(_m), 3);

    void *old = nullptr;
    EXPECT_EQ(HDQL_AVL_CHANGED, hdql_fmap_erase(_m, k10.data(), &old));
    EXPECT_EQ(&v10, old);
    EXPECT_EQ(hdql_fmap_size(_m), 2);

    EXPECT_EQ(nullptr, hdql_fmap_get(_m, k10.data()));
    EXPECT_EQ(&v20, hdql_fmap_get(_m, k20.data()));
    EXPECT_EQ(&v30, hdql_fmap_get(_m, k30.data()));
}

TEST_F(TestAVLTree, eraseNodeWithTwoChildren) {
    std::vector<uint64_t> keys = {
        40, 20, 60, 10, 30, 50, 70
    };

    std::vector<int> values(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = static_cast<int>(keys[i]);
        auto k = key_u64(keys[i]);
        ASSERT_EQ(0, hdql_fmap_insert(_m, k.data(), &values[i]));
        EXPECT_EQ(hdql_fmap_size(_m), i + 1);
    }

    auto k40 = key_u64(40);

    void *old = nullptr;
    EXPECT_EQ(1, hdql_fmap_erase(_m, k40.data(), &old));
    EXPECT_EQ(&values[0], old);
    EXPECT_EQ(hdql_fmap_size(_m), 6);

    EXPECT_EQ(nullptr, hdql_fmap_get(_m, k40.data()));

    for (size_t i = 1; i < keys.size(); ++i) {
        auto k = key_u64(keys[i]);
        EXPECT_EQ(&values[i], hdql_fmap_get(_m, k.data()));
    }
}

TEST_F(TestAVLTree, eraseMissingKeyReturnsZero) {
    int value = 123;
    auto k1 = key_u64(1);
    auto k2 = key_u64(2);

    ASSERT_EQ(0, hdql_fmap_insert(_m, k1.data(), &value));
    EXPECT_EQ(hdql_fmap_size(_m), 1);

    void *old = reinterpret_cast<void *>(0xdeadbeef);
    EXPECT_EQ(0, hdql_fmap_erase(_m, k2.data(), &old));

    /* old should remain untouched if key was absent */
    EXPECT_EQ(reinterpret_cast<void *>(0xdeadbeef), old);
    EXPECT_EQ(&value, hdql_fmap_get(_m, k1.data()));
    EXPECT_EQ(hdql_fmap_size(_m), 1);
}

/*
 * AVL iteration
 */

struct IterState {
    std::vector<uint64_t> keys;
    std::vector<void *> values;
};

static uint64_t be_to_u64(const unsigned char *k) {
    uint64_t x = 0;

    for (int i = 0; i < 8; ++i) {
        x <<= 8;
        x |= static_cast<uint64_t>(k[i]);
    }

    return x;
}

static int collect_iter_cb(
        const unsigned char *key,
        size_t keyLen,
        void **value,
        void *userdata)
{
    auto *st = static_cast<IterState *>(userdata);

    EXPECT_EQ(8u, keyLen);

    st->keys.push_back(be_to_u64(key));
    st->values.push_back(*value);

    return 0;
}

TEST_F(TestAVLTree, IteratesInSortedOrder) {
    std::vector<uint64_t> keys = {
        5, 1, 9, 3, 7, 2, 8
    };

    std::vector<int> values(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = static_cast<int>(keys[i]);
        auto k = key_u64(keys[i]);
        ASSERT_EQ(0, hdql_fmap_insert(_m, k.data(), &values[i]));
    }

    IterState st;
    EXPECT_EQ(0, hdql_fmap_iter(_m, collect_iter_cb, &st));

    std::vector<uint64_t> expected = keys;
    std::sort(expected.begin(), expected.end());

    EXPECT_EQ(expected, st.keys);
}

struct StopState {
    int count = 0;
};

static int stop_after_three_cb(
        const unsigned char *key,
        size_t keyLen,
        void **value,
        void *userdata)
{
    (void) key;
    (void) keyLen;
    (void) value;

    auto *st = static_cast<StopState *>(userdata);

    ++st->count;

    if (st->count == 3)
        return 1234;

    return 0;
}

TEST_F(TestAVLTree, iterationCanStopEarly) {
    int values[10];

    for (uint64_t i = 0; i < 10; ++i) {
        values[i] = static_cast<int>(i);
        auto k = key_u64(i);
        ASSERT_EQ(0, hdql_fmap_insert(_m, k.data(), &values[i]));
    }

    StopState st;

    EXPECT_EQ(1234, hdql_fmap_iter(_m, stop_after_three_cb, &st));
    EXPECT_EQ(3, st.count);
}

struct ReplaceState {
    int replacement = 777;
};

static int replace_even_keys_cb(
        const unsigned char *key,
        size_t keyLen,
        void **value,
        void *userdata)
{
    (void) keyLen;

    auto *st = static_cast<ReplaceState *>(userdata);

    uint64_t k = be_to_u64(key);

    if ((k % 2) == 0)
        *value = &st->replacement;

    return 0;
}

TEST_F(TestAVLTree, iterationCallbackMayReplaceValues) {
    int values[6];

    for (uint64_t i = 0; i < 6; ++i) {
        values[i] = static_cast<int>(i);
        auto k = key_u64(i);
        ASSERT_EQ(0, hdql_fmap_insert(_m, k.data(), &values[i]));
    }

    ReplaceState st;

    EXPECT_EQ(0, hdql_fmap_iter(_m, replace_even_keys_cb, &st));

    for (uint64_t i = 0; i < 6; ++i) {
        auto k = key_u64(i);

        if ((i % 2) == 0)
            EXPECT_EQ(&st.replacement, hdql_fmap_get(_m, k.data()));
        else
            EXPECT_EQ(&values[i], hdql_fmap_get(_m, k.data()));
    }
}

