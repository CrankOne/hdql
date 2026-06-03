// This app tests HDQL implementations of hash table and AVL tree versus
// general-purpose STL containers.
//
// Assumed usage:
//      $ python ../hdql/drafts/run.py | tee ./results.dat
//      $ gnuplot
//  gnuplot>  set log xy
//  gnuplot> plot 'results.dat' using 1:5 with linespoints title "Map", '' using 1:9 with linespoints title "UMap Lookup", '' using 1:13 w linespoints t 'HT'

#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include "hdql/util/allocator.h"
#include "hdql/types.h"
#include "hdql/context.h"
#include "hdql/util/avl-tree.h"
#include "hdql/util/ht.h"

// Timing utils
using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double>;

double elapsed_seconds(std::chrono::time_point<Clock> start, std::chrono::time_point<Clock> end) {
    return std::chrono::duration_cast<Duration>(end - start).count();
}

// Key/value generators

// randomized, high entropy
std::string random_string(size_t min_len = 1, size_t max_len = 128) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
    std::uniform_int_distribution<size_t> char_dist(0, sizeof(charset) - 2);

    size_t len = len_dist(rng);
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        result += charset[char_dist(rng)];
    }
    return result;
}


// hash table benchmark
void benchmark_hash_table(const std::vector<std::string> &keys, const std::vector<int> &values) {
    // NOTE: somehow, strlen() in the ht_s... functions seem to
    // significantly slowdown this benchmark. For more fair comparison,
    // let's use the C++ size() as done in the RB tree.

    hdql_Allocator alloc = hdql_gHeapAllocator;
    struct hdql_ht * ht = hdql_ht_create(&alloc, 8, HDQL_MURMUR3_32_DEFAULT_SEED);
    std::vector<int *> vs;

    auto startInsert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        #if 0
        hdql_ht_s_ins(ht, keys[i].c_str(), reinterpret_cast<void*>(values[i]), NULL);
        #else
        hdql_ht_ins(ht, (const unsigned char *) keys[i].c_str(), keys[i].size(), reinterpret_cast<void*>(values[i]), NULL);
        #endif
    }
    auto endInsert = Clock::now();

    size_t hits = 0;
    auto startLookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        #if 0
        long unsigned int val
            = reinterpret_cast<long unsigned int>(hdql_ht_s_get(ht, keys[i].c_str()));
        #else
        long unsigned int val
            = reinterpret_cast<long unsigned int>(hdql_ht_get(ht, (const unsigned char *) keys[i].c_str(), keys[i].size()));
        #endif
        if (((int) val) == values[i]) ++hits;
    }
    auto endLookup = Clock::now();

    std::cout << "hdql_ht insert time:   " << elapsed_seconds(startInsert, endInsert) << " sec\n";
    std::cout << "hdql_ht lookup time:   " << elapsed_seconds(startLookup, endLookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";

    hdql_ht_destroy(ht);

    for(auto ptr: vs) alloc.free(ptr, alloc.userdata);
}

// std::map benchmark
void benchmark_std_map(const std::vector<std::string> &keys, const std::vector<int> &values) {
    std::map<std::string, int> m;

    auto startInsert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        m[keys[i]] = values[i];
    auto endInsert = Clock::now();

    size_t hits = 0;
    auto startLookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        if (m[keys[i]] == values[i]) ++hits;
    auto endLookup = Clock::now();

    std::cout << "std::map insert time:      " << elapsed_seconds(startInsert, endInsert) << " sec\n";
    std::cout << "std::map lookup time:      " << elapsed_seconds(startLookup, endLookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";
}

// std::unordered_map benchmark
void benchmark_std_unordered_map(const std::vector<std::string> &keys, const std::vector<int> &values) {
    std::unordered_map<std::string, int> m;

    auto startInsert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        m[keys[i]] = values[i];
    auto endInsert = Clock::now();

    size_t hits = 0;
    auto startLookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        if (m[keys[i]] == values[i]) ++hits;
    auto endLookup = Clock::now();

    std::cout << "std::unordered_map insert time: " << elapsed_seconds(startInsert, endInsert) << " sec\n";
    std::cout << "std::unordered_map lookup time: " << elapsed_seconds(startLookup, endLookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";
}

// HDQL variadic length key benchmark
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
void benchmark_hdql_vmap(const std::vector<std::string> &keys, const std::vector<int> &values) {
    hdql_vmap *m = hdql_vmap_create(cstr_key_cmp, cstr_key_len, &hdql_gHeapAllocator);

    auto startInsert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        hdql_vmap_insert(m, keys[i].data(), const_cast<int*>(values.data() + i));
    auto endInsert = Clock::now();

    size_t hits = 0;
    auto startLookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        int *r = reinterpret_cast<int*>(hdql_vmap_get(m, keys[i].data()));
        if (*r == values[i]) ++hits;
    }
    auto endLookup = Clock::now();

    std::cout << "hdql_vmap insert time: " << elapsed_seconds(startInsert, endInsert) << " sec\n";
    std::cout << "hdql_vmap lookup time: " << elapsed_seconds(startLookup, endLookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";
}

// Entry point
//

int main(int argc, char **argv) {
    size_t count = 4;
    if (argc > 1) count = std::stoul(argv[1]);

    std::cout << "# Benchmarking with " << count << " items.\n";

    std::vector<std::string> keys;
    std::vector<int> values;
    keys.reserve(count);
    values.reserve(count);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> val_dist(0, 1 << 30);

    for (size_t i = 0; i < count; ++i) {
        keys.push_back(random_string(4, 64));
        values.push_back(val_dist(rng));
    }

    benchmark_std_map(keys, values);
    benchmark_std_unordered_map(keys, values);
    benchmark_hash_table(keys, values);
    benchmark_hdql_vmap(keys, values);

    return 0;
}

