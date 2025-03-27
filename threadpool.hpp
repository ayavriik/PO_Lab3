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

    // ThreadPool: приймає кіл-ть робітників і час очікування -> передає у канал виконання
    ThreadPool(size_t size, std::chrono::seconds sleep_duration)
        : stop_flag(false)
        , sleep_duration(sleep_duration) // ініціалізація поля класу
    {
        // створення каналів
        channel = std::make_shared<Channel<Job>>(); // для передачі завдань від планувальника до робітників
        backChannel = std::make_shared<Channel<std::pair<size_t, uint64_t>>>(); // для збору статистики від робітників

        // створення робітників
        for (size_t id = 0; id < size; ++id) { // Worker, який отримує свій унікальний id
            workers.emplace_back(new Worker(id, channel, backChannel)); // спільний канал завдань та канал для зворотного зв'язку
        }

        // створення планувальника
        scheduler = std::make_shared<Scheduler>(channel);

        // нескінченний запускк циклу потоку планувальника 
        scheduler_thread = std::thread([this]() {
            // звертаємось до sleep_duration через this-указівник
            std::this_thread::sleep_for(this->sleep_duration);

            while (true) {
                // час початку циклу для подальшого вимірювання часу виконання
                auto start_time = std::chrono::steady_clock::now();

                // переносить завдання з буфера в канал та повертає кіл-ть завдань, що були перенесені
                uint64_t tasks = scheduler->run();

                // збір інформації від робітників через зворотний канал
                uint64_t done = 0;
                while (done < tasks) {
                    auto result = backChannel->receive();
                    {
                        std::lock_guard<std::mutex> lock(waiting_mtx);
                        workers_waiting_times.push_back(result);
                    }
                    ++done;
                }

                // обчислення часу для виконання поточного пакету завдань
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                uint64_t execution_time = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                {
                    std::lock_guard<std::mutex> lock(exec_mtx);
                    queues_execution_times.push_back(execution_time);
                }

                std::cout << "Tasks were processing for " << execution_time << " seconds" << std::endl;

                 // обчислення залишкового часу сну до наступного циклу (якщо час, що залишився, більше нуля, планувальник засинає)
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
                    // якщо вже не лишилось часу для сну — потік звільняється
                    std::this_thread::yield();
                }

                std::cout << "Scheduler is running" << std::endl;

                // перевірка, чи треба зупиняти планувальник
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

    // деструктор
    ~ThreadPool() {
        // join(); (щоб уникнути подвійного приєднання потоків)
    }

   // дозволяє додавати завдання до пулу, використовуючи метод планувальника
    void execute(Job job) {
        scheduler->schedule(std::move(job));
    }

    // методи для призупинення та відновлення роботи планувальника
    void pause() {
        scheduler->pause();
    }

    void unpause() {
        scheduler->unpause();
    }

    // метод stop() встановлює прапорець stop_flag та сигналізує про завершення робботи
    void stop() {
        std::lock_guard<std::mutex> lock(stop_mtx);
        stop_flag = true;
    }

    // метод join(): викликає stop() -> приєднує потік планувальника -> надсилає сигнал зупинки робітникам -> приєднує потоки співробітників
    void join() {
        stop();
        if (scheduler_thread.joinable()) {
            scheduler_thread.join();
        }

        // надсилання сигналу зупинки всім робітникам 
        for (size_t i = 0; i < workers.size(); ++i) {
            channel->send(Job());
        }

        // приєднуєлнання до всіх робітників
        for (auto &worker : workers) {
            worker->join();
        }

        print_stats();
    }

    // метод detach() від'єднує потік планувальника та робітників, а потім виводить статистику
    void detach() {
        stop();
        if (scheduler_thread.joinable()) {
            scheduler_thread.detach();
        }
        for (auto &worker : workers) {
            worker->detach();
        }
        print_stats();
    }

    // додаткові гетери для перевірки стану каналів і буфера
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
    std::vector<std::unique_ptr<Worker>> workers; // контейнер для зберігання робітників
    std::shared_ptr<Channel<Job>> channel; // канал для передачі завдань до робітників
    std::shared_ptr<Channel<std::pair<size_t, uint64_t>>> backChannel; // канал для отримання статистики від робітників
    std::shared_ptr<Scheduler> scheduler; // планувальник, який управляє буфером завдань

    std::thread scheduler_thread; // потік, у якому працює планувальник
    bool stop_flag; // прапорець для сигналу зупинки роботи планувальника
    std::mutex stop_mtx; // м'ютекс для синхронізації доступу до stop_flag

    // поле для інтервалу очікування 
    std::chrono::seconds sleep_duration;

    // Дані для збору статистики
    std::vector<std::pair<size_t, uint64_t>> workers_waiting_times; // зберігає для кожного робітника час очікування завдання
    std::vector<uint64_t> queues_execution_times; // зберігає час виконання кожного "пакету" завдань
    std::mutex waiting_mtx; // м'ютекс для синхронізації доступу до статистики часу очікування
    std::mutex exec_mtx; // м'ютекс для синхронізації доступу до статистики часу виконання

    void print_stats() {
        std::lock_guard<std::mutex> lock(waiting_mtx);
        std::cout << "Worker waiting times:" << std::endl;
        // групування часу очікування за ідентифікатором робітника
        std::unordered_map<size_t, std::vector<uint64_t>> total_waiting_time;

        // обчислення середнього часу очікування для кожного робітника
        for (const auto &entry : workers_waiting_times) {
            total_waiting_time[entry.first].push_back(entry.second);
        }

        for (const auto &entry : total_waiting_time) {
            uint64_t sum = 0;
            for (auto t : entry.second) {
                sum += t;
            }
            double avg = static_cast<double>(sum) / entry.second.size();
            std::cout << "Worker " << entry.first << " average waiting time " << avg << " seconds" << std::endl;
        }

        // блокування для безпечного доступу до статистики часу виконання
        std::lock_guard<std::mutex> lock2(exec_mtx);
        if (!queues_execution_times.empty()) {
            uint64_t sum = 0;
            for (auto t : queues_execution_times) {
                sum += t;
            }
            double avg = static_cast<double>(sum) / queues_execution_times.size();
            std::cout << "Average queue execution time: " << avg << " seconds" << std::endl;
        }
    }
};

#endif // THREADPOOL_HPP
