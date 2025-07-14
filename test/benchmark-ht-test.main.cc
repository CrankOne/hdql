// Usage:
//  $ make
//  $ python ../hdql/drafts/run.py | tee ./results.dat
//  $ gnuplot
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

#include "hdql/allocator.h"
#include "hdql/types.h"
#include "hdql/context.h"
#include "hdql/hash-table.h"

// --- Timing utilities
using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double>;

double elapsed_seconds(std::chrono::time_point<Clock> start, std::chrono::time_point<Clock> end) {
    return std::chrono::duration_cast<Duration>(end - start).count();
}

// --- Key/value generators
#if 1
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
#else
// Global prefix pool (simulates shared path structure)
std::vector<std::string> prefix_pool = {
    "a", "ab", "abc", "abd", "abe",
    "b", "bc", "bcd", "bce", "bcf",
    "c", "cd", "cde", "cdf", "cxyz"
};

std::string random_string(size_t min_len = 4, size_t max_len = 32) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> pick_prefix(0, prefix_pool.size() - 1);
    std::uniform_int_distribution<> extra_len_dist(0, max_len - min_len);

    std::string prefix = prefix_pool[pick_prefix(gen)];
    size_t remaining = min_len > prefix.size() ? (min_len - prefix.size()) : 0;
    remaining += extra_len_dist(gen);  // pad to total length

    std::string suffix;
    suffix.reserve(remaining);
    for (size_t i = 0; i < remaining; ++i)
        suffix += charset[gen() % (sizeof(charset) - 1)];

    return prefix + suffix;
}
#endif

#if 0
// --- Radix Tree wrapper
void benchmark_hdql_rt(const std::vector<std::string> &keys, const std::vector<int> &values) {
    hdql_Context_t ctx = hdql_context_create(0x0);

    struct hdql_rt *tree = hdql_rt_new(ctx);

    auto start_insert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        int *val = (int *)hdql_context_alloc(ctx, sizeof(int));
        *val = values[i];
        hdql_rt_insert(tree, keys[i].c_str(), val);
    }
    auto end_insert = Clock::now();

    size_t hits = 0;
    auto start_lookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        int *val = (int *)hdql_rt_lookup(tree, keys[i].c_str());
        if (val && *val == values[i]) ++hits;
    }
    auto end_lookup = Clock::now();

    std::cout << "hdql_rt insert time:   " << elapsed_seconds(start_insert, end_insert) << " sec\n";
    std::cout << "hdql_rt lookup time:   " << elapsed_seconds(start_lookup, end_lookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";

    hdql_rt_destroy(tree);
    hdql_context_destroy(ctx);
}
#endif

// --- hash table benchmark
void benchmark_hash_table(const std::vector<std::string> &keys, const std::vector<int> &values) {
    hdql_Allocator alloc = hdql_gHeapAllocator;
    //hdql_alloc_arena_init(&alloc);
    struct hdql_ht * ht = hdql_ht_create(&alloc, 5, HDQL_MURMUR3_32_DEFAULT_SEED);
    std::vector<int *> vs;

    auto start_insert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        //int *val = (int *) malloc(sizeof(int));
        //int *val = (int *) alloc.alloc(sizeof(int), alloc.userdata);
        //vs.push_back(val);
        //*val = values[i];
        //hdql_ht_s_ins(ht, keys[i].c_str(), val);
        hdql_ht_s_ins(ht, keys[i].c_str(), reinterpret_cast<void*>(values[i]));
    }
    auto end_insert = Clock::now();

    size_t hits = 0;
    auto start_lookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        //int *val = (int *) hdql_ht_s_get(ht, keys[i].c_str());
        //int *val = (int *) hdql_ht_get(ht, (const unsigned char *) keys[i].c_str(), keys[i].size());
        //if (val && *val == values[i]) ++hits;
        long unsigned int val
            = reinterpret_cast<long unsigned int>(hdql_ht_get(ht, (const unsigned char *) keys[i].c_str(), keys[i].size()));
        if (((int) val) == values[i]) ++hits;
    }
    auto end_lookup = Clock::now();

    std::cout << "hdql_ht insert time:   " << elapsed_seconds(start_insert, end_insert) << " sec\n";
    std::cout << "hdql_ht lookup time:   " << elapsed_seconds(start_lookup, end_lookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";

    hdql_ht_destroy(ht);

    for(auto ptr: vs) alloc.free(ptr, alloc.userdata);
    //hdql_alloc_arena_destroy(&alloc);
}

// --- std::map benchmark
void benchmark_std_map(const std::vector<std::string> &keys, const std::vector<int> &values) {
    std::map<std::string, int> m;

    auto start_insert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        m[keys[i]] = values[i];
    auto end_insert = Clock::now();

    size_t hits = 0;
    auto start_lookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        if (m[keys[i]] == values[i]) ++hits;
    auto end_lookup = Clock::now();

    std::cout << "std::map insert time:      " << elapsed_seconds(start_insert, end_insert) << " sec\n";
    std::cout << "std::map lookup time:      " << elapsed_seconds(start_lookup, end_lookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";
}

// --- std::unordered_map benchmark
void benchmark_std_unordered_map(const std::vector<std::string> &keys, const std::vector<int> &values) {
    std::unordered_map<std::string, int> m;

    auto start_insert = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        m[keys[i]] = values[i];
    auto end_insert = Clock::now();

    size_t hits = 0;
    auto start_lookup = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i)
        if (m[keys[i]] == values[i]) ++hits;
    auto end_lookup = Clock::now();

    std::cout << "std::unordered_map insert time: " << elapsed_seconds(start_insert, end_insert) << " sec\n";
    std::cout << "std::unordered_map lookup time: " << elapsed_seconds(start_lookup, end_lookup) << " sec\n";
    std::cout << "  (hits: " << hits << ")\n";
}

// --- Main entry point
int main(int argc, char **argv) {
    size_t count = 100000;
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
    //benchmark_hdql_rt(keys, values);
    benchmark_hash_table(keys, values);

    return 0;
}

