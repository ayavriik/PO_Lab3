#ifndef WORKER_HPP
#define WORKER_HPP

#include <thread>
#include <functional>
#include <iostream>
#include <memory>
#include "channel.hpp"

// визначення типу Job (як функція без параметрів))
using Job = std::function<void()>;

class Worker {
public:

    // приймає ідентифікатор робітника та спільний вказівник на Channel, що має завдання
    Worker(size_t id, std::shared_ptr<Channel<Job>> channel)
        : id(id), channel(channel), thread(&Worker::run, this)
    {
    }

    // метод для приєднання (join) потоку для його завершення
    void join() {
        if (thread.joinable())
            thread.join();
    }

    // метод для від'єднання потоку, якщо необхідно
    void detach() {
        if (thread.joinable())
            thread.detach();
    }

private:
    size_t id; // ідентифікатор робітника
    std::shared_ptr<Channel<Job>> channel; // спільний вказівник на канал із завданнями
    std::thread thread; // потік, в якому працює робітник


    // метод, який запускається у потоці і постійно очікує завдання з каналу для виконання
    void run() {
        while (true) {

            // отримання завдання з каналу (блокується, якщо черга порожня)
            Job job = channel->receive();

            // якщо отримане завдання (false) - це сигнал до зупинки робітника
            if (!job) {
                std::cout << "Worker " << id << " was told to stop." << std::endl;
                break;
            }

            // логування факту отримання завдання та його виконання
            std::cout << "Worker " << id << " got a job; executing." << std::endl;
            job();
        }
    }
};

#endif // WORKER_HPP
