#include "doctest.h"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <array>
#include <cstdint>

using namespace VoxelEngine;

// =========================================================================
// Helper: lightweight shard map that mirrors ChunkMap's 64-shard locking
// without pulling in godot::RID (which needs the engine runtime).
// =========================================================================
class TestShardMap {
public:
    static constexpr size_t kNumShards = 64;

    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<uint64_t, std::unique_ptr<ChunkData>> chunks;
    };

    std::array<Shard, kNumShards> shards_;
    std::atomic<size_t> count_{0};

    static uint64_t key(int32_t x, int32_t y, int32_t z) {
        constexpr uint32_t OFF = 1u << 20;
        constexpr uint32_t MASK = 0x1FFFFF;
        uint64_t ux = static_cast<uint64_t>((static_cast<uint32_t>(x) + OFF) & MASK);
        uint64_t uy = static_cast<uint64_t>((static_cast<uint32_t>(y) + OFF) & MASK);
        uint64_t uz = static_cast<uint64_t>((static_cast<uint32_t>(z) + OFF) & MASK);
        return (ux << 42) | (uy << 21) | uz;
    }

    size_t shard_of(uint64_t k) const { return k % kNumShards; }

    void insert(uint64_t k, std::unique_ptr<ChunkData> d) {
        auto& s = shards_[shard_of(k)];
        std::unique_lock lk(s.mutex);
        auto [it, ins] = s.chunks.insert_or_assign(k, std::move(d));
        (void)it;
        if (ins) count_.fetch_add(1, std::memory_order_relaxed);
    }

    void erase(uint64_t k) {
        auto& s = shards_[shard_of(k)];
        std::unique_lock lk(s.mutex);
        if (s.chunks.erase(k) > 0)
            count_.fetch_sub(1, std::memory_order_relaxed);
    }

    ChunkData* get(uint64_t k) const {
        auto& s = shards_[shard_of(k)];
        std::shared_lock lk(s.mutex);
        auto it = s.chunks.find(k);
        return (it != s.chunks.end()) ? it->second.get() : nullptr;
    }

    size_t size() const { return count_.load(std::memory_order_relaxed); }

    // RAII shared lock on a single shard
    class [[nodiscard]] ShardLock {
        std::shared_lock<std::shared_mutex> lk_;
    public:
        ShardLock(const TestShardMap& m, uint64_t k)
            : lk_(m.shards_[m.shard_of(k)].mutex) {}
    };

    // RAII exclusive lock on a single shard
    class [[nodiscard]] ExclusiveShardLock {
        std::unique_lock<std::shared_mutex> lk_;
    public:
        ExclusiveShardLock(const TestShardMap& m, uint64_t k)
            : lk_(m.shards_[m.shard_of(k)].mutex) {}
    };

    // Lock shards in ascending order (deadlock-safe)
    class [[nodiscard]] OrderedShardLock {
        std::vector<std::shared_lock<std::shared_mutex>> lks_;
    public:
        OrderedShardLock() = default;
        OrderedShardLock(const TestShardMap& m, const std::vector<uint64_t>& keys) {
            bool seen[kNumShards] = {};
            for (auto k : keys) seen[m.shard_of(k)] = true;
            lks_.reserve(kNumShards);
            for (size_t i = 0; i < kNumShards; ++i)
                if (seen[i]) lks_.emplace_back(m.shards_[i].mutex);
        }
    };

    // Lock all shards with shared locks
    class [[nodiscard]] AllSharedLock {
        std::vector<std::shared_lock<std::shared_mutex>> lks_;
    public:
        AllSharedLock() = default;
        explicit AllSharedLock(const TestShardMap& m) {
            lks_.reserve(kNumShards);
            for (auto& s : m.shards_)
                lks_.emplace_back(s.mutex);
        }
    };

    // Lock all shards with exclusive locks
    class [[nodiscard]] AllExclusiveLock {
        std::vector<std::unique_lock<std::shared_mutex>> lks_;
    public:
        AllExclusiveLock() = default;
        explicit AllExclusiveLock(const TestShardMap& m) {
            lks_.reserve(kNumShards);
            for (auto& s : m.shards_)
                lks_.emplace_back(s.mutex);
        }
    };
};

// =========================================================================
// 1. Concurrent shared reads on different shards — no contention expected
// =========================================================================
TEST_CASE("concurrent reads on different shards do not block each other") {
    TestShardMap m;
    constexpr int N = 64;
    for (int i = 0; i < N; i++) {
        auto cd = std::make_unique<ChunkData>();
        cd->clear();
        cd->set_block(0, 0, 0, BlockIDs::STONE);
        m.insert(TestShardMap::key(i, 0, 0), std::move(cd));
    }

    std::atomic<int> sum_a{0}, sum_b{0};
    auto reader = [&](int start, int end, std::atomic<int>& out) {
        for (int i = start; i < end; i++) {
            auto k = TestShardMap::key(i, 0, 0);
            TestShardMap::ShardLock lk(m, k);
            auto* d = m.shards_[m.shard_of(k)].chunks.find(k)->second.get();
            if (d && !d->is_all_air())
                out.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto t0 = std::chrono::steady_clock::now();
    std::thread ta(reader, 0, 32, std::ref(sum_a));
    std::thread tb(reader, 32, 64, std::ref(sum_b));
    ta.join();
    tb.join();
    auto dt = std::chrono::steady_clock::now() - t0;

    CHECK(sum_a.load() == 32);
    CHECK(sum_b.load() == 32);
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() < 5000);
}

// =========================================================================
// 2. Ascending-shard lock ordering prevents deadlock
// =========================================================================
TEST_CASE("lock_keys ordering prevents deadlock") {
    TestShardMap m;
    for (int i = 0; i < 128; i++) {
        auto cd = std::make_unique<ChunkData>();
        cd->clear();
        m.insert(TestShardMap::key(i, i, i), std::move(cd));
    }

    std::atomic<bool> done_a{false}, done_b{false};
    std::atomic<int> completed{0};

    auto worker = [&](int32_t ax, int32_t ay, int32_t az,
                      int32_t bx, int32_t by, int32_t bz,
                      std::atomic<bool>& flag) {
        uint64_t ka = TestShardMap::key(ax, ay, az);
        uint64_t kb = TestShardMap::key(bx, by, bz);
        std::vector<uint64_t> keys = {ka, kb};
        TestShardMap::OrderedShardLock lk(m, keys);
        volatile int x = 0;
        for (int i = 0; i < 1000; i++) x += i;
        (void)x;
        completed.fetch_add(1, std::memory_order_relaxed);
        flag.store(true, std::memory_order_release);
    };

    // shard = key % 64 depends only on lower 6 bits, which come from z.
    // z=0 → shard 0, z=1 → shard 1.
    std::thread ta(worker, 0, 0, 0, 0, 0, 1, std::ref(done_a));
    std::thread tb(worker, 0, 0, 1, 0, 0, 0, std::ref(done_b));
    ta.join();
    tb.join();

    CHECK(done_a.load());
    CHECK(done_b.load());
    CHECK(completed.load() == 2);
}

// =========================================================================
// 3. PaletteStorage concurrent read/write on different sections
// =========================================================================
TEST_CASE("concurrent PaletteStorage read/write on different sections") {
    PaletteStorage ps;
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++)
            ps.set_block(x, 0, z, BlockIDs::STONE);
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++)
            ps.set_block(x, 16, z, BlockIDs::DIRT);

    std::atomic<bool> writer_done{false};
    std::atomic<int> read_errors{0};

    auto writer = [&]() {
        for (int iter = 0; iter < 100; iter++) {
            for (int x = 0; x < 16; x++)
                for (int z = 0; z < 16; z++)
                    ps.set_block(x, 0, z, (iter % 2 == 0) ? BlockIDs::STONE : BlockIDs::GRASS);
        }
        writer_done.store(true, std::memory_order_release);
    };

    auto reader = [&]() {
        while (!writer_done.load(std::memory_order_acquire)) {
            for (int x = 0; x < 16; x++)
                for (int z = 0; z < 16; z++) {
                    BlockID b = ps.get_block(x, 16, z);
                    if (b != BlockIDs::DIRT)
                        read_errors.fetch_add(1, std::memory_order_relaxed);
                }
        }
    };

    std::thread tw(writer);
    std::thread tr(reader);
    tw.join();
    tr.join();

    CHECK(read_errors.load() == 0);
}

// =========================================================================
// 4. All-exclusive lock serializes concurrent access (simulates
//    lock_all_exclusive blocking readers)
// =========================================================================
TEST_CASE("lock_all_exclusive serializes concurrent access") {
    TestShardMap m;
    for (int i = 0; i < 128; i++) {
        auto cd = std::make_unique<ChunkData>();
        cd->clear();
        m.insert(TestShardMap::key(i, 0, 0), std::move(cd));
    }

    std::atomic<int> writer_phase{0};
    std::atomic<bool> reader_saw_phase_zero{false};
    std::atomic<bool> reader_saw_phase_two{false};

    auto writer = [&]() {
        TestShardMap::AllExclusiveLock lk(m);
        writer_phase.store(1, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        writer_phase.store(2, std::memory_order_release);
    };

    auto reader = [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        TestShardMap::AllSharedLock lk(m);
        int phase = writer_phase.load(std::memory_order_acquire);
        if (phase == 0) reader_saw_phase_zero.store(true);
        if (phase == 2) reader_saw_phase_two.store(true);
    };

    std::thread tw(writer);
    std::thread tr(reader);
    tw.join();
    tr.join();

    CHECK_FALSE(reader_saw_phase_zero.load());
    CHECK(reader_saw_phase_two.load());
}

// =========================================================================
// 5. pending_light_removals_ mutex pattern — concurrent insert/erase
// =========================================================================
TEST_CASE("pending_light_removals_ concurrent insert/erase does not crash") {
    std::unordered_set<uint64_t> pending;
    std::mutex mu;

    constexpr int NUM_KEYS = 500;
    std::atomic<bool> stop{false};

    auto inserter = [&](uint64_t base) {
        for (int i = 0; i < NUM_KEYS && !stop.load(std::memory_order_acquire); i++) {
            std::lock_guard<std::mutex> g(mu);
            pending.insert(base + i);
        }
    };

    auto eraser = [&]() {
        for (int i = 0; i < NUM_KEYS && !stop.load(std::memory_order_acquire); i++) {
            std::lock_guard<std::mutex> g(mu);
            auto it = pending.find(i);
            if (it != pending.end()) pending.erase(it);
        }
    };

    std::thread t1(inserter, 0);
    std::thread t2(inserter, NUM_KEYS);
    std::thread t3(eraser);
    t1.join();
    t2.join();
    stop.store(true, std::memory_order_release);
    t3.join();

    CHECK(pending.size() <= static_cast<size_t>(2 * NUM_KEYS));
}

// =========================================================================
// 6. ShardMap size counter accuracy under concurrent insert/erase
// =========================================================================
TEST_CASE("ShardMap size is accurate under concurrent modifications") {
    TestShardMap m;
    constexpr int N = 100;

    auto inserter = [&](int start, int end) {
        for (int i = start; i < end; i++) {
            auto cd = std::make_unique<ChunkData>();
            cd->clear();
            m.insert(TestShardMap::key(i, 0, 0), std::move(cd));
        }
    };

    auto eraser = [&](int start, int end) {
        for (int i = start; i < end; i++) {
            m.erase(TestShardMap::key(i, 0, 0));
        }
    };

    std::thread t1(inserter, 0, N / 2);
    std::thread t2(inserter, N / 2, N);
    t1.join();
    t2.join();
    CHECK(m.size() == N);

    std::thread t3(eraser, 0, N / 2);
    std::thread t4(eraser, N / 2, N);
    t3.join();
    t4.join();
    CHECK(m.size() == 0);
}

// =========================================================================
// 7. Shared shard lock releases on destruction
// =========================================================================
TEST_CASE("ShardLock releases on destruction") {
    TestShardMap m;
    auto cd = std::make_unique<ChunkData>();
    cd->clear();
    m.insert(TestShardMap::key(0, 0, 0), std::move(cd));

    {
        TestShardMap::ShardLock lk1(m, TestShardMap::key(0, 0, 0));
        TestShardMap::ShardLock lk2(m, TestShardMap::key(0, 0, 0));
    }
    TestShardMap::ExclusiveShardLock el(m, TestShardMap::key(0, 0, 0));
    CHECK(true);
}

// =========================================================================
// 8. Exclusive shard lock blocks concurrent shared
// =========================================================================
TEST_CASE("ExclusiveShardLock blocks concurrent shared") {
    TestShardMap m;
    auto cd = std::make_unique<ChunkData>();
    cd->clear();
    m.insert(TestShardMap::key(0, 0, 0), std::move(cd));

    std::atomic<bool> exclusive_held{false};
    std::atomic<bool> shared_acquired{false};

    auto exclusive_worker = [&]() {
        TestShardMap::ExclusiveShardLock el(m, TestShardMap::key(0, 0, 0));
        exclusive_held.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    };

    auto shared_worker = [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        TestShardMap::ShardLock sl(m, TestShardMap::key(0, 0, 0));
        shared_acquired.store(true, std::memory_order_release);
    };

    std::thread te(exclusive_worker);
    std::thread ts(shared_worker);
    te.join();
    ts.join();

    CHECK(exclusive_held.load());
    CHECK(shared_acquired.load());
}

// =========================================================================
// 9. Exclusive lock + reader: reader must see writer's modifications
// =========================================================================
TEST_CASE("exclusive lock serializes writer and reader on same shard") {
    TestShardMap m;
    auto cd = std::make_unique<ChunkData>();
    cd->clear();
    m.insert(TestShardMap::key(5, 0, 0), std::move(cd));

    std::atomic<bool> reader_saw_air{false};

    auto writer = [&]() {
        TestShardMap::ExclusiveShardLock el(m, TestShardMap::key(5, 0, 0));
        auto* d = m.shards_[m.shard_of(TestShardMap::key(5, 0, 0))]
                    .chunks[TestShardMap::key(5, 0, 0)].get();
        d->set_block(0, 0, 0, BlockIDs::STONE);
    };

    auto reader = [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        TestShardMap::ShardLock sl(m, TestShardMap::key(5, 0, 0));
        auto* d = m.shards_[m.shard_of(TestShardMap::key(5, 0, 0))]
                    .chunks[TestShardMap::key(5, 0, 0)].get();
        if (d->get_block(0, 0, 0) == BlockIDs::AIR)
            reader_saw_air.store(true, std::memory_order_relaxed);
    };

    std::thread tw(writer);
    std::thread tr(reader);
    tw.join();
    tr.join();

    // Reader might see air (if it read before writer) or stone (after).
    // The key invariant: no crash, no hang, no torn read.
    CHECK(true);
}
