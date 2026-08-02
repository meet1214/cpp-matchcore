#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <vector>
class ThreadPool{
    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> task_queue_;
        std::mutex queue_mutex_;
        std::condition_variable cv_;
        std::atomic<bool> stopped_;
        void worker_loop(){
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    cv_.wait(lock, [this]{
                        return stopped_.load(std::memory_order_relaxed) || !task_queue_.empty();
                    });
                    
                    if(stopped_.load(std::memory_order_relaxed) && task_queue_.empty()){
                        return;
                    }
                    task = task_queue_.front();
                    task_queue_.pop();
                }
                task();
            }
        }
    
    public:
        explicit ThreadPool(std::size_t thread_count){
            
            if(thread_count == 0){
                throw std::invalid_argument("ThreadPool: thread_count must be > 0"); 
            }

            stopped_ = false;

            workers_.reserve(thread_count);
            for(std::size_t i = 0; i < thread_count; i++){
                workers_.emplace_back([this]{ worker_loop(); });
            }

        }

        ~ThreadPool(){
            shutdown();
        }

        void shutdown(){
            
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if(stopped_.exchange(true, std::memory_order_acq_rel)){
                return;
            }

            lock.unlock();

            cv_.notify_all();

            for(auto& t : workers_ ){
                if(t.joinable()){
                    t.join();
                }
            }
        }

        bool is_stopped() const noexcept{
            return stopped_.load(std::memory_order_acquire);
        }

        std::size_t thread_count() const noexcept{
            return workers_.size();
        }

        template<typename F, typename... Args>
        auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>{

            using ReturnType = std::invoke_result_t<F, Args...>;
            if(stopped_.load(std::memory_order_acquire)){
                throw std::runtime_error("ThreadPool: submit() called after shutdown");
            }

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [f = std::forward<F>(f),
                args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                    return std::apply(std::move(f), std::move(args));
                }
            );

            auto fut = task->get_future(); 

            std::unique_lock<std::mutex> lock(queue_mutex_);
            if(stopped_){
                throw std::runtime_error("ThreadPool: submit() called after shutdown");
            }
            task_queue_.emplace([task](){ (*task)(); });
            lock.unlock();
            cv_.notify_one();
            return fut;
        }

        
};