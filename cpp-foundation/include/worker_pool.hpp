#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <queue>
#include <condition_variable>
#include <optional>

namespace cortex {

class WorkerPool {
public:
    explicit WorkerPool(size_t max_workers = 4)
        : max_workers_(max_workers) {}

    ~WorkerPool() {
        shutdown();
    }

    // Submit a synchronous task; executes on an existing or new worker.
    template<typename Fn>
    auto submit(Fn&& fn) -> decltype(fn()) {
        using R = decltype(fn());
        std::packaged_task<R()> task(std::forward<Fn>(fn));
        auto fut = task.get_future();
        {
            std::unique_lock<std::mutex> lk(mu_);
            ensure_worker_unlocked();
            tasks_.emplace([t = std::move(task)]() mutable { t(); });
        }
        cv_.notify_one();
        return fut.get();
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lk(mu_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

private:
    void ensure_worker_unlocked() {
        if (workers_.size() < max_workers_ && idle_guess_ == 0) {
            workers_.emplace_back([this](){
                worker_loop();
            });
        }
    }

    void worker_loop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(mu_);
                ++idle_guess_;
                cv_.wait(lk, [&]{ return stopping_ || !tasks_.empty(); });
                --idle_guess_;
                if (stopping_ && tasks_.empty()) return;
                job = std::move(tasks_.front());
                tasks_.pop();
            }
            try { job(); } catch (...) { /* swallow to keep pool alive */ }
        }
    }

    size_t max_workers_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::atomic<bool> stopping_{false};
    size_t idle_guess_{0};
};

} // namespace cortex