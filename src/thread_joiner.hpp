#pragma once

#include <cstddef>
#include <cstdlib>
#include <thread>
#include <vector>

namespace zkgpt {

// Worker-thread budget shared by the data-parallel loops: ZKGPT_THREADS if set
// to a positive integer, otherwise every hardware thread.
inline std::size_t workerThreadCount() {
    static const std::size_t count=[] {
        if (const char *value=std::getenv("ZKGPT_THREADS")) {
            const long parsed=std::strtol(value,nullptr,10);
            if (parsed>0) return static_cast<std::size_t>(parsed);
        }
        const unsigned hardware=std::thread::hardware_concurrency();
        return static_cast<std::size_t>(hardware ? hardware : 1);
    }();
    return count;
}

// std::thread's destructor terminates the process while the thread is still
// joinable.  Keep this guard immediately after a thread vector so a partial
// worker-pool construction is also joined during stack unwinding.
class ThreadJoiner {
public:
    explicit ThreadJoiner(std::vector<std::thread> &threads) noexcept
        : threads_(threads) {}

    ThreadJoiner(const ThreadJoiner &) = delete;
    ThreadJoiner &operator=(const ThreadJoiner &) = delete;

    ~ThreadJoiner() noexcept { join(); }

    void join() noexcept {
        for (auto &thread : threads_)
            if (thread.joinable()) thread.join();
    }

private:
    std::vector<std::thread> &threads_;
};

}  // namespace zkgpt
