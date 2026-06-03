#include <gtest/gtest.h>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "hdql/util/allocator.h"
#include "hdql/util/ht.h"
#include "hdql/context.h"
#include "hdql/types.h"

static void *
_hdql_test_alloc(size_t sz, void * chunks_) {
    auto & chunks = *reinterpret_cast<std::unordered_set<void *>*>(chunks_);
    assert(sz > 0);
    void * newData = malloc(sz);
    assert(newData);
    chunks.emplace(newData);
    return newData;
}

static int
_hdql_test_free(void * ptr, void * chunks_) {
    auto & chunks = *reinterpret_cast<std::unordered_set<void *>*>(chunks_);
    assert(ptr);
    auto it = chunks.find(ptr);
    if(it == chunks.end()) {
        throw std::runtime_error("free requested for unknown ptr");
    }
    chunks.erase(it);
    free(ptr);
    return 0;
}

class HDQLHashTable : public ::testing::Test {
protected:
    hdql_Allocator _alloc;
public:
    void SetUp() override {
        _alloc.userdata = new std::unordered_set<void *>();
        _alloc.alloc = _hdql_test_alloc;
        _alloc.free = _hdql_test_free;
    }
    void TearDown() override {
        auto * regPtr = reinterpret_cast<std::unordered_set<void *>*>(_alloc.userdata);
        EXPECT_TRUE(regPtr->empty()) << "still has " << regPtr->size()
            << " blocks allocated.";
        delete regPtr;
    }
};

TEST_F(HDQLHashTable, basicInsertAndLookup) {
    hdql_ht * ht = hdql_ht_create(&_alloc, 5, HDQL_MURMUR3_32_DEFAULT_SEED);

    int v1 = 42;
    int v2 = 1337;

    void *oldValue = NULL;
    EXPECT_EQ(HDQL_HT_RC_INSERTED, hdql_ht_s_ins(ht, "abc",  &v1, &oldValue));
    EXPECT_FALSE(oldValue);
    EXPECT_EQ(HDQL_HT_RC_INSERTED, hdql_ht_s_ins(ht, "abcd", &v2, NULL));

    void* found1 = hdql_ht_s_get(ht, "abc");
    void* found2 = hdql_ht_s_get(ht, "abcd");

    ASSERT_EQ(*reinterpret_cast<int*>(found1), 42);
    ASSERT_EQ(*reinterpret_cast<int*>(found2), 1337);

    hdql_ht_destroy(ht);
}

TEST_F(HDQLHashTable, lookupNonExistentKey) {
    hdql_ht * ht = hdql_ht_create(&_alloc, 5, HDQL_MURMUR3_32_DEFAULT_SEED);

    int v = 100;
    EXPECT_EQ(HDQL_HT_RC_INSERTED, hdql_ht_s_ins(ht, "hello", &v, NULL));

    void * result = hdql_ht_s_get(ht, "world");
    EXPECT_EQ(result, nullptr);

    hdql_ht_destroy(ht);
}

TEST_F(HDQLHashTable, overwriteExistingKey) {
    hdql_ht * ht = hdql_ht_create(&_alloc, 5, HDQL_MURMUR3_32_DEFAULT_SEED);

    int v1 = 1;
    int v2 = 2;

    void *oldValue = NULL;
    EXPECT_EQ(HDQL_HT_RC_INSERTED, hdql_ht_s_ins(ht, "key", &v1, &oldValue));
    EXPECT_FALSE(oldValue);
    EXPECT_EQ(HDQL_HT_RC_UPDATED,  hdql_ht_s_ins(ht, "key", &v2, &oldValue));  // overwrite
    EXPECT_EQ(oldValue, &v1);

    void* result = hdql_ht_s_get(ht, "key");
    ASSERT_EQ(*reinterpret_cast<int*>(result), 2);

    hdql_ht_destroy(ht);
}

TEST_F(HDQLHashTable, removeKey) {
    hdql_ht * ht = hdql_ht_create(&_alloc, 5, HDQL_MURMUR3_32_DEFAULT_SEED);

    int value = 999;
    EXPECT_EQ(HDQL_HT_RC_INSERTED, hdql_ht_s_ins(ht, "tokill", &value, NULL));
    EXPECT_EQ(hdql_ht_s_get(ht, "tokill"), &value);

    void *popVal = NULL;
    EXPECT_EQ(HDQL_HT_RC_OK, hdql_ht_s_rm(ht, "tokill", &popVal));
    EXPECT_EQ(popVal, &value);
    popVal = NULL;
    EXPECT_EQ(HDQL_HT_RC_ERR_NOENT, hdql_ht_s_rm(ht, "tokill", &popVal));
    EXPECT_EQ(NULL, hdql_ht_s_get(ht, "tokill"));
    EXPECT_FALSE(popVal);

    hdql_ht_destroy(ht);
}

TEST_F(HDQLHashTable, iteration) {
    hdql_ht * ht = hdql_ht_create(&_alloc, 5, HDQL_MURMUR3_32_DEFAULT_SEED);

    int a = 1, b = 2, c = 3;
    hdql_ht_s_ins(ht, "a", &a, NULL);
    hdql_ht_s_ins(ht, "b", &b, NULL);
    hdql_ht_s_ins(ht, "c", &c, NULL);

    struct VisitData {
        std::set<std::string> keysVisited;
    } v;

    auto visit = [](const unsigned char * key, size_t keyLen, void ** value, void * userdata) {
        VisitData* vd = static_cast<VisitData*>(userdata);
        vd->keysVisited.insert(std::string(key, key + keyLen - 1));
        return 0;
    };

    int rc = hdql_ht_iter(ht, visit, &v);

    EXPECT_EQ(0, rc);
    EXPECT_EQ(v.keysVisited.size(), 3);
    EXPECT_EQ(1, v.keysVisited.count("a"));
    EXPECT_EQ(1, v.keysVisited.count("b"));
    EXPECT_EQ(1, v.keysVisited.count("c"));

    hdql_ht_destroy(ht);
}

class HDQLHashTableStressTest : public HDQLHashTable {
protected:
    hdql_ht * ht;
    void SetUp() override {
        HDQLHashTable::SetUp();
        ht = hdql_ht_create(&_alloc, 5, HDQL_MURMUR3_32_DEFAULT_SEED);
        ASSERT_NE(ht, nullptr);
    }

    void TearDown() override {
        if (ht) hdql_ht_destroy(ht);
        HDQLHashTable::TearDown();
    }
};

TEST_F(HDQLHashTableStressTest, insertsLookupsDeletionsAndIteration) {
    constexpr size_t numKeys = 200;
    std::unordered_map<std::string, int> expectedMap;
    std::vector<std::string> insertedKeys;
    char keyBuffer[64];
    std::vector<int *> values;  // container to be emptied later

    // 1. Insert 200 keys of varying structure
    for (size_t i = 0; i < numKeys; ++i) {
        if (i % 3 == 0)
            snprintf(keyBuffer, sizeof(keyBuffer), "x_%zu", i);
        else if (i % 5 == 0)
            snprintf(keyBuffer, sizeof(keyBuffer), "long_prefix_xyz_sub_%04zu", i);
        else
            snprintf(keyBuffer, sizeof(keyBuffer), "%zu", i * 13);

        std::string key = keyBuffer;
        int *val = (int*) malloc(sizeof(int));
        values.push_back(val);
        *val = static_cast<int>(i);
        insertedKeys.push_back(key);
        expectedMap[key] = *val;

        void *oldValue = NULL;
        int rc = hdql_ht_s_ins(ht, key.c_str(), val, &oldValue);
        EXPECT_EQ(rc, HDQL_HT_RC_INSERTED)
            << "Unexpected insertion exit code: " << rc;
        EXPECT_FALSE(oldValue);
    }

    // 2. Remove every 7th inserted key
    std::set<std::string> removedKeys;
    for (size_t i = 0; i < insertedKeys.size(); i += 7) {
        void *oldValue = NULL;
        const std::string &key = insertedKeys[i];
        int status = hdql_ht_s_rm(ht, key.c_str(), &oldValue);
        EXPECT_EQ(status, HDQL_HT_RC_OK) << "Expected to remove key: "
            << key << ", got rc=" << status;
        EXPECT_TRUE(oldValue);
        removedKeys.insert(key);
        expectedMap.erase(key);
    }

    // 3. Lookup remaining keys
    for (const auto &[key, expectedVal] : expectedMap) {
        void *ptr = hdql_ht_s_get(ht, key.c_str());
        ASSERT_NE(ptr, nullptr) << "Key not found: " << key;
        int *ival = static_cast<int *>(ptr);
        EXPECT_EQ(*ival, expectedVal) << "Mismatched value for key: " << key;
    }

    // 4. Confirm removed keys are gone
    for (const auto &key : removedKeys) {
        void *ptr = hdql_ht_s_get(ht, key.c_str());
        EXPECT_EQ(ptr, nullptr) << "Expected key to be removed: " << key;
    }

    // 5. Add some new keys after removals
    for (size_t i = 1000; i < 1020; ++i) {
        snprintf(keyBuffer, sizeof(keyBuffer), "postfix_key_%zu", i);
        std::string key = keyBuffer;
        int * val = (int*) malloc(sizeof(int));
        values.push_back(val);
        expectedMap[key] = *val;
        void *oldValue = NULL;
        int rc = hdql_ht_s_ins(ht, key.c_str(), val, &oldValue);
        EXPECT_EQ(rc, HDQL_HT_RC_INSERTED);
        EXPECT_FALSE(oldValue);
    }

    // 6. Check that iteration sees all and only remaining keys
    std::set<std::string> iteratedKeys;
    auto visit = [](const unsigned char * key, size_t keyLen, void ** value, void * userdata) {
        auto *keySet = static_cast<std::set<std::string> *>(userdata);
        keySet->insert(std::string(key, key + keyLen - 1));
        return 0;
    };
    hdql_ht_iter(ht, visit, &iteratedKeys);

    EXPECT_EQ(iteratedKeys.size(), expectedMap.size());
    for (const auto &pair : expectedMap) {
        EXPECT_TRUE(iteratedKeys.count(pair.first)) << "Missing in iteration: " << pair.first;
    }

    for(auto vPtr : values) free(vPtr);
}

