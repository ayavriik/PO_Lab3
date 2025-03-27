#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <deque>
#include <mutex>
#include <functional>
#include <memory>
#include "channel.hpp"

using Job = std::function<void()>;

class Scheduler {
public:
    // конструктор приймає спільний вказівник на канал завдань
    Scheduler(std::shared_ptr<Channel<Job>> channel)
        : channel(channel)
    {
    }

    // додає завдання в буфер
    void schedule(Job job) {
        std::lock_guard<std::mutex> lock(mtx);
        buffer.push_back(std::move(job));
    }

    // переміщує всі завдання з буфера в канал
    // повертає кількість перенесених завдань
    uint64_t run() {
        std::lock_guard<std::mutex> lock(mtx);
        uint64_t size = buffer.size();
        while (!buffer.empty()) {
            Job job = std::move(buffer.front());
            buffer.pop_front();
            channel->send(std::move(job));
        }
        return size;
    }

private:
    std::deque<Job> buffer;               // буфер для накопичення завдань
    std::mutex mtx;                       // м'ютекс для синхронізації доступу до буфера
    std::shared_ptr<Channel<Job>> channel; // спільний канал для передачі завдань
};

#endif // SCHEDULER_HPP
