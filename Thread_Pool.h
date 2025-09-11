#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> job;

                    { // Lock job queue
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return !jobs.empty() || stop;
                        });

                        if (stop && jobs.empty()) return;

                        job = std::move(jobs.front());
                        jobs.pop();
                    }

                    job(); // run job
                }
            });
        }
    }

    ~ThreadPool() {
        {   // signal stop
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& w : workers) w.join();
    }

    void enqueue(std::function<void()> job) {
        {   // add job
            std::unique_lock<std::mutex> lock(queue_mutex);
            jobs.push(std::move(job));
        }
        condition.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;
};
