#include <gtest/gtest.h>

#include "hdql/util/avl-tree.h"
#include "hdql/util/allocator.h"

#include <cstring>

namespace {
int cstr_key_cmp(const void *a, const void *b) {
    return std::strcmp(
            static_cast<const char *>(a),
            static_cast<const char *>(b));
}

size_t cstr_key_len(const void *a) {
    return std::strlen(static_cast<const char *>(a)) + 1;
}
}  // anon ns

class AVLVarKeyMap : public ::testing::Test {
protected:
    const hdql_Allocator * _allocator;
    hdql_vmap *_m;
public:
    AVLVarKeyMap() : _allocator(&hdql_gHeapAllocator), _m(nullptr) {}
    void SetUp() override {
        _m = hdql_vmap_create(cstr_key_cmp, cstr_key_len, _allocator);
    }

    void TearDown() override {
        if(_m) hdql_vmap_destroy(_m);
    }
};

TEST_F(AVLVarKeyMap, emptyLookupReturnsNull) {
    EXPECT_EQ(nullptr, hdql_vmap_get(_m, "alpha"));
}

TEST_F(AVLVarKeyMap, insertAndLookupSingleValue) {
    int value = 42;

    EXPECT_EQ(0, hdql_vmap_insert(_m, "alpha", &value));
    EXPECT_EQ(&value, hdql_vmap_get(_m, "alpha"));
}

TEST_F(AVLVarKeyMap, insertSeveralAndLookupAll) {
    int va = 1;
    int vb = 2;
    int vc = 3;

    EXPECT_EQ(0, hdql_vmap_insert(_m, "gamma", &vc));
    EXPECT_EQ(0, hdql_vmap_insert(_m, "alpha", &va));
    EXPECT_EQ(0, hdql_vmap_insert(_m, "beta",  &vb));

    EXPECT_EQ(&va, hdql_vmap_get(_m, "alpha"));
    EXPECT_EQ(&vb, hdql_vmap_get(_m, "beta"));
    EXPECT_EQ(&vc, hdql_vmap_get(_m, "gamma"));

    EXPECT_EQ(nullptr, hdql_vmap_get(_m, "delta"));
}

TEST_F(AVLVarKeyMap, replacesExistingValue) {
    int v1 = 10;
    int v2 = 20;

    EXPECT_EQ(0, hdql_vmap_insert(_m, "alpha", &v1));
    EXPECT_EQ(1, hdql_vmap_insert(_m, "alpha", &v2));

    EXPECT_EQ(&v2, hdql_vmap_get(_m, "alpha"));
}

TEST_F(AVLVarKeyMap, keyIsCopiedOnInsert) {
    char key[] = "alpha";
    int value = 42;

    EXPECT_EQ(0, hdql_vmap_insert(_m, key, &value));

    std::strcpy(key, "omega");

    EXPECT_EQ(&value, hdql_vmap_get(_m, "alpha"));
    EXPECT_EQ(nullptr, hdql_vmap_get(_m, "omega"));
}

TEST_F(AVLVarKeyMap, handlesPrefixKeysAsDistinct) {
    int va = 1;
    int vab = 2;
    int vabc = 3;

    EXPECT_EQ(0, hdql_vmap_insert(_m, "a",   &va));
    EXPECT_EQ(0, hdql_vmap_insert(_m, "ab",  &vab));
    EXPECT_EQ(0, hdql_vmap_insert(_m, "abc", &vabc));

    EXPECT_EQ(&va,   hdql_vmap_get(_m, "a"));
    EXPECT_EQ(&vab,  hdql_vmap_get(_m, "ab"));
    EXPECT_EQ(&vabc, hdql_vmap_get(_m, "abc"));
}


TEST_F(AVLVarKeyMap, eraseMissingKeyReturnsZero) {
    int value = 42;

    EXPECT_EQ(0, hdql_vmap_insert(_m, "alpha", &value));

    void *old = reinterpret_cast<void *>(0xdeadbeef);

    EXPECT_EQ(0, hdql_vmap_erase(_m, "beta", &old));
    EXPECT_EQ(reinterpret_cast<void *>(0xdeadbeef), old);

    EXPECT_EQ(&value, hdql_vmap_get(_m, "alpha"));
}

TEST_F(AVLVarKeyMap, eraseLeafNode) {
    int va = 1;
    int vb = 2;
    int vc = 3;

    ASSERT_EQ(0, hdql_vmap_insert(_m, "beta",  &vb));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "alpha", &va));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "gamma", &vc));

    void *old = nullptr;

    EXPECT_EQ(1, hdql_vmap_erase(_m, "alpha", &old));
    EXPECT_EQ(&va, old);

    EXPECT_EQ(nullptr, hdql_vmap_get(_m, "alpha"));
    EXPECT_EQ(&vb, hdql_vmap_get(_m, "beta"));
    EXPECT_EQ(&vc, hdql_vmap_get(_m, "gamma"));
}

TEST_F(AVLVarKeyMap, eraseNodeWithTwoChildren) {
    int v_d = 4;
    int v_b = 2;
    int v_f = 6;
    int v_a = 1;
    int v_c = 3;
    int v_e = 5;
    int v_g = 7;

    ASSERT_EQ(0, hdql_vmap_insert(_m, "d", &v_d));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "b", &v_b));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "f", &v_f));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "a", &v_a));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "c", &v_c));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "e", &v_e));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "g", &v_g));

    void *old = nullptr;

    EXPECT_EQ(1, hdql_vmap_erase(_m, "d", &old));
    EXPECT_EQ(&v_d, old);

    EXPECT_EQ(nullptr, hdql_vmap_get(_m, "d"));

    EXPECT_EQ(&v_a, hdql_vmap_get(_m, "a"));
    EXPECT_EQ(&v_b, hdql_vmap_get(_m, "b"));
    EXPECT_EQ(&v_c, hdql_vmap_get(_m, "c"));
    EXPECT_EQ(&v_e, hdql_vmap_get(_m, "e"));
    EXPECT_EQ(&v_f, hdql_vmap_get(_m, "f"));
    EXPECT_EQ(&v_g, hdql_vmap_get(_m, "g"));
}

namespace {
struct IterState {
    std::vector<std::string> keys;
    std::vector<void *> values;
};

int collect_cb(
        const void *key,
        void **value,
        void *userdata) {
    auto *st = static_cast<IterState *>(userdata);

    const char *s = static_cast<const char *>(key);

    st->keys.emplace_back(s);
    st->values.push_back(*value);

    return 0;
}
}  // anon ns

TEST_F(AVLVarKeyMap, iteratesInSortedOrder) {
    std::vector<std::string> keys = {
        "delta", "alpha", "charlie", "bravo", "echo"
    };

    std::vector<int> values(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = static_cast<int>(i);
        ASSERT_EQ(0, hdql_vmap_insert(_m, keys[i].c_str(), &values[i]));
    }

    IterState st;

    EXPECT_EQ(0, hdql_vmap_iter(_m, collect_cb, &st));

    std::vector<std::string> expected = keys;
    std::sort(expected.begin(), expected.end());

    EXPECT_EQ(expected, st.keys);
}

namespace {
struct StopState {
    int n = 0;
};

int stop_after_three_cb(
        const void *key,
        void **value,
        void *userdata) {
    (void) key;
    (void) value;

    auto *st = static_cast<StopState *>(userdata);

    ++st->n;

    return st->n == 3 ? 1234 : 0;
}
}  // anon ns

TEST_F(AVLVarKeyMap, IterationCanStopEarly) {
    int values[10];

    for (int i = 0; i < 10; ++i) {
        values[i] = i;

        std::string key = "k" + std::to_string(i);
        ASSERT_EQ(0, hdql_vmap_insert(_m, key.c_str(), &values[i]));
    }

    StopState st;

    EXPECT_EQ(1234, hdql_vmap_iter(_m, stop_after_three_cb, &st));
    EXPECT_EQ(3, st.n);
}

namespace {
struct ReplaceState {
    int replacement = 777;
};

int replace_b_keys_cb(
        const void *key,
        void **value,
        void *userdata) {

    auto *st = static_cast<ReplaceState *>(userdata);
    const char *s = static_cast<const char *>(key);

    if (s[0] == 'b')
        *value = &st->replacement;

    return 0;
}
}  // anon ns

TEST_F(AVLVarKeyMap, iterationCallbackMayReplaceValue) {
    int va = 1;
    int vb = 2;
    int vc = 3;

    ASSERT_EQ(0, hdql_vmap_insert(_m, "alpha", &va));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "beta",  &vb));
    ASSERT_EQ(0, hdql_vmap_insert(_m, "gamma", &vc));

    ReplaceState st;

    EXPECT_EQ(0, hdql_vmap_iter(_m, replace_b_keys_cb, &st));

    EXPECT_EQ(&va, hdql_vmap_get(_m, "alpha"));
    EXPECT_EQ(&st.replacement, hdql_vmap_get(_m, "beta"));
    EXPECT_EQ(&vc, hdql_vmap_get(_m, "gamma"));
}


TEST_F(AVLVarKeyMap, manyInsertEraseLookupOperations) {
    constexpr int N = 10000;

    std::vector<std::string> keys;
    std::vector<int> values(N);

    keys.reserve(N);

    for (int i = 0; i < N; ++i) {
        keys.push_back("key_" + std::to_string(i));
        values[i] = i;

        ASSERT_EQ(0, hdql_vmap_insert(_m, keys.back().c_str(), &values[i]));
    }

    for (int i = 0; i < N; ++i)
        EXPECT_EQ(&values[i], hdql_vmap_get(_m, keys[i].c_str()));

    for (int i = 0; i < N; i += 2) {
        void *old = nullptr;

        EXPECT_EQ(1, hdql_vmap_erase(_m, keys[i].c_str(), &old));
        EXPECT_EQ(&values[i], old);
    }

    for (int i = 0; i < N; ++i) {
        if ((i % 2) == 0)
            EXPECT_EQ(nullptr, hdql_vmap_get(_m, keys[i].c_str()));
        else
            EXPECT_EQ(&values[i], hdql_vmap_get(_m, keys[i].c_str()));
    }
}
