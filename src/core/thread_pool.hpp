#ifndef FUK_MINECRAFT_VOXEL_ENGINE_THREAD_POOL_HPP
#define FUK_MINECRAFT_VOXEL_ENGINE_THREAD_POOL_HPP
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <type_traits>
#include <queue>
#include <thread>
#include <vector>

namespace VoxelEngine {

struct Task {
    virtual ~Task() = default;
    virtual void execute() = 0;
};

template<typename F>
struct FnTask : Task {
    explicit FnTask(F f) : fn_(std::move(f)) {}
    F fn_;
    void execute() override { fn_(); }
};

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads = default_thread_count())
        : stop_flag_(false), next_worker_(0)
    {
        num_threads = std::max(std::size_t(1), num_threads);
        workers_.reserve(num_threads);

        for (std::size_t i = 0; i < num_threads; ++i) {
            queues_.emplace_back();
            workers_.emplace_back([this, i] { worker_loop(i); });
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template<typename F>
    void fire_and_forget(F&& f) {
        enqueue_task(std::make_unique<FnTask<std::decay_t<F>>>(std::forward<F>(f)));
    }

    void enqueue_task(std::unique_ptr<Task> task, bool high_priority = false) {
        if (stop_flag_.load(std::memory_order_acquire))
            throw std::runtime_error("enqueue_task on stopped ThreadPool");

        std::size_t idx = next_worker_.fetch_add(1, std::memory_order_relaxed) % queues_.size();
        auto& q = queues_[idx];
        {
            std::unique_lock<std::mutex> lock(q.mtx);
            if (high_priority)
                q.high_pri.emplace(std::move(task));
            else
                q.normal.emplace(std::move(task));
        }
        q.cv.notify_one();
    }

    [[nodiscard]] std::size_t get_queue_size() const {
        std::size_t total = 0;
        for (auto& q : queues_) {
            std::unique_lock<std::mutex> lock(q.mtx);
            total += q.high_pri.size() + q.normal.size();
        }
        return total;
    }

    [[nodiscard]] std::size_t get_high_priority_queue_size() const {
        std::size_t total = 0;
        for (auto& q : queues_) {
            std::unique_lock<std::mutex> lock(q.mtx);
            total += q.high_pri.size();
        }
        return total;
    }

    [[nodiscard]] std::size_t get_worker_count() const noexcept {
        return workers_.size();
    }

    void shutdown() {
        if (stop_flag_.exchange(true))
            return;
        for (auto& q : queues_)
            q.cv.notify_all();
        for (auto& w : workers_) {
            if (w.joinable())
                w.join();
        }
    }

private:
    struct PerWorker {
        std::queue<std::unique_ptr<Task>> high_pri;
        std::queue<std::unique_ptr<Task>> normal;
        mutable std::mutex mtx;
        std::condition_variable cv;
    };

    void worker_loop(std::size_t idx) {
        auto& q = queues_[idx];
        while (true) {
            std::unique_ptr<Task> task;
            {
                std::unique_lock<std::mutex> lock(q.mtx);
                q.cv.wait(lock, [this, &q] {
                    return stop_flag_.load(std::memory_order_acquire) ||
                           !q.high_pri.empty() || !q.normal.empty();
                });
                if (stop_flag_.load(std::memory_order_acquire) &&
                    q.high_pri.empty() && q.normal.empty())
                    return;
                if (!q.high_pri.empty()) {
                    task = std::move(q.high_pri.front());
                    q.high_pri.pop();
                } else {
                    task = std::move(q.normal.front());
                    q.normal.pop();
                }
            }
            task->execute();
        }
    }

    static std::size_t default_thread_count() noexcept {
        std::size_t hc = std::thread::hardware_concurrency();
        return (hc > 1) ? (hc - 1) : 1;
    }

    std::vector<std::thread> workers_;
    std::deque<PerWorker> queues_;
    std::atomic<bool> stop_flag_;
    std::atomic<std::size_t> next_worker_;
};

} // namespace VoxelEngine

#endif
