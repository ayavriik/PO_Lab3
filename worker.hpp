#ifndef WORKER_HPP
#define WORKER_HPP

#include <thread>
#include <functional>
#include <chrono>
#include <iostream>
#include <memory>
#include "channel.hpp"

// тип Job як функція без параметрів, що повертає void
using Job = std::function<void()>;

// конструктор класу Worker, що приймає: id, channel, backChannel
// worker надсилає інформацію про час очікування завдання через спільний вказівник на канал для зворотного зв'язку
class Worker {
public:
    Worker(size_t id,
           std::shared_ptr<Channel<Job>> channel,
           std::shared_ptr<Channel<std::pair<size_t, uint64_t>>> backChannel)
        : id(id), channel(channel), backChannel(backChannel), thread(&Worker::run, this)
    {
    }

    // метод join(), дозволяє основному потоку чекати завершення роботи цього worker'а
    void join() {
        if (thread.joinable())
            thread.join();
    }

    // метод detach() від'єднує потік worker'а, дозволяючи йому працювати незалежно
    void detach() {
        if (thread.joinable())
            thread.detach();
    }

private:
    size_t id; // унікальний ідентифікатор worker'а
    std::shared_ptr<Channel<Job>> channel; // спільний вказівник на канал завдань
    std::shared_ptr<Channel<std::pair<size_t, uint64_t>>> backChannel; // канал для зворотного зв'язку (id, час очікування)
    std::thread thread; // потік, в якому працює worker

    // метод run() отримує завдання з каналу, вимірює час очікування, виконує завдання
    // потім надсилає статистику (ідентифікатор і час очікування) через backChannel
    void run() {
        while (true) {
            auto start_time = std::chrono::steady_clock::now();

            // отримання завдання з каналу (блокується, якщо черга порожня)
            Job job = channel->receive();
            auto wait_time = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();

            // якщо отримане завдання порожнє -> зупинка роботи worker'а
            if (!job) {
                std::cout << "Worker " << id << " was told to stop." << std::endl;
                break;
            }

            std::cout << "Worker " << id << " got a job; executing." << std::endl;
            job();

            backChannel->send(std::make_pair(id, static_cast<uint64_t>(wait_time)));
        }
    }
};

#endif // WORKER_HPP
