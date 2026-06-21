#ifndef FUK_MINECRAFT_VOXEL_ENGINE_THREAD_POOL_HPP
#define FUK_MINECRAFT_VOXEL_ENGINE_THREAD_POOL_HPP
#include <condition_variable>
#include <cstddef>
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
        : stop_flag_(false)
    {
        num_threads = std::max(std::size_t(1), num_threads);
        workers_.reserve(num_threads);

        for (std::size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
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
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_flag_) {
                throw std::runtime_error("enqueue_task on stopped ThreadPool");
            }
            if (high_priority) {
                high_priority_tasks_.emplace(std::move(task));
            } else {
                tasks_.emplace(std::move(task));
            }
        }
        condition_.notify_one();
    }

    [[nodiscard]] std::size_t get_queue_size() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size() + high_priority_tasks_.size();
    }

    [[nodiscard]] std::size_t get_high_priority_queue_size() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return high_priority_tasks_.size();
    }

    [[nodiscard]] std::size_t get_worker_count() const noexcept {
        return workers_.size();
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (std::exchange(stop_flag_, true)) {
                return;
            }
        }
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    void worker_loop() {
        while (true) {
            std::unique_ptr<Task> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    return stop_flag_ || !high_priority_tasks_.empty() || !tasks_.empty();
                });

                if (stop_flag_ && high_priority_tasks_.empty() && tasks_.empty()) {
                    return;
                }

                // Drain high-priority queue first
                if (!high_priority_tasks_.empty()) {
                    task = std::move(high_priority_tasks_.front());
                    high_priority_tasks_.pop();
                } else {
                    task = std::move(tasks_.front());
                    tasks_.pop();
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
    std::queue<std::unique_ptr<Task>> tasks_;
    std::queue<std::unique_ptr<Task>> high_priority_tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_flag_;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_VOXEL_ENGINE_THREAD_POOL_HPP