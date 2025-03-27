#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <functional>
#include <mutex>
#include <memory>
#include <unordered_map>
#include "channel.hpp"
#include "scheduler.hpp"
#include "worker.hpp"

using Job = std::function<void()>;

class ThreadPool {
public:
    ThreadPool(size_t size, std::chrono::seconds sleep_duration)
        : stop_flag(false)
        , sleep_duration(sleep_duration)
    {
        // створення каналів для завдань і зворотного зв'язку
        channel = std::make_shared<Channel<Job>>();
        backChannel = std::make_shared<Channel<std::pair<size_t, uint64_t>>>();

        // створення робітників
        for (size_t id = 0; id < size; ++id) {
            workers.emplace_back(new Worker(id, channel, backChannel));
        }

        // створення планувальника
        scheduler = std::make_shared<Scheduler>(channel);

        // запуск потоку планувальника, який переносить завдання з буфера в канал
        scheduler_thread = std::thread([this]() {
            // початкова затримка за умовою (sleep_duration)
            std::this_thread::sleep_for(this->sleep_duration);

            while (true) {
                auto start_time = std::chrono::steady_clock::now();

                // виклик методу run() планувальника для перенесення завдань
                uint64_t tasks = scheduler->run();

                // збір статистики: очікування робітників через зворотний канал
                uint64_t done = 0;
                while (done < tasks) {
                    auto result = backChannel->receive();
                    {
                        std::lock_guard<std::mutex> lock(waiting_mtx);
                        workers_waiting_times.push_back(result);
                    }
                    ++done;
                }

                // вимірювання часу виконання завдань у цьому циклі
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                uint64_t execution_time = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                {
                    std::lock_guard<std::mutex> lock(exec_mtx);
                    queues_execution_times.push_back(execution_time);
                }

                std::cout << "Tasks were processing for " << execution_time << " seconds" << std::endl;

                // обчислення залишкового часу сну до наступного циклу
                auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start_time
                );
                auto remaining_sleep = (this->sleep_duration > elapsed_sec)
                    ? (this->sleep_duration - elapsed_sec)
                    : std::chrono::seconds(0);

                if (remaining_sleep.count() > 0) {
                    std::cout << "Scheduler is sleeping for " << remaining_sleep.count() << " seconds" << std::endl;
                    std::this_thread::sleep_for(remaining_sleep);
                } else {
                    // Якщо часу на сон не залишилося, звільняємо потік
                    std::this_thread::yield();
                }

                std::cout << "Scheduler is running" << std::endl;

                // перевірка прапорця stop_flag для завершення роботи планувальника
                {
                    std::lock_guard<std::mutex> lock(stop_mtx);
                    if (stop_flag) {
                        std::cout << "Scheduler is stopping" << std::endl;
                        break;
                    }
                }
            }
        });
    }

    // гетери залишаються незмінними
    bool is_empty() {
        return channel->is_empty();
    }

    size_t queue_size() {
        return channel->len();
    }

    bool is_buffer_empty() {
        return scheduler->is_empty();
    }

    size_t buffer_size() {
        return scheduler->size();
    }

private:
    std::vector<std::unique_ptr<Worker>> workers;
    std::shared_ptr<Channel<Job>> channel;
    std::shared_ptr<Channel<std::pair<size_t, uint64_t>>> backChannel;
    std::shared_ptr<Scheduler> scheduler;

    std::thread scheduler_thread;
    bool stop_flag;
    std::mutex stop_mtx;

    // поле для інтервалу очікування (наприклад, 45 секунд за умовою)
    std::chrono::seconds sleep_duration;

    // контейнери для збору статистики
    std::vector<std::pair<size_t, uint64_t>> workers_waiting_times;
    std::vector<uint64_t> queues_execution_times;
    std::mutex waiting_mtx;
    std::mutex exec_mtx;
};

#endif // THREADPOOL_HPP
