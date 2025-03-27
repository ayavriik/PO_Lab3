#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <deque>
#include <mutex>
#include <functional>
#include <memory>
#include <iostream>
#include "channel.hpp"

// тип Job як функцію, що не приймає параметрів і повертає void.
using Job = std::function<void()>;

// приймає спільний вказівник на канал завдань
// ініціалізує прапорець ready в true (планувальник готовий переносити завдання)
class Scheduler {
public:
    Scheduler(std::shared_ptr<Channel<Job>> channel)
        : channel(channel), ready(true) {}

    // метод pause() для призупинення перенесення завдань з буфера в канал
    // захищає доступ до прапорця ready за допомогою м'ютекса
    void pause() {
        std::lock_guard<std::mutex> lock(mtx);
        ready = false;
    }

    // метод unpause(): відновлює перенесення завдань, встановлюючи ready в true
    void unpause() {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    }

    // метод run():перевірка на дозвіл перенесення завдання
    uint64_t run() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!ready)
            return 0;
        uint64_t size = buffer.size();
        std::cout << "Running " << size << " jobs" << std::endl;

        // перебираємо всі завдання у буфері та переміщаємо їх у канал
        while (!buffer.empty()) {
            // std::move для ефективного переміщення завдання без зайвого копіювання
            Job job = std::move(buffer.front());
            buffer.pop_front(); // видалення завдання з буфера після його вилучення
            channel->send(std::move(job)); // передача завдання до каналу для подальшого виконання робітниками
        }
        return size;
    }

    // метод schedule(): додає нове завдання до внутрішнього буфера
    // захищає доступ до буфера за допомогою м'ютекса
    void schedule(Job job) {
        std::lock_guard<std::mutex> lock(mtx);
        buffer.push_back(std::move(job));
    }

    // метод size(): повертає поточну кількість завдань, що знаходяться в буфері
    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return buffer.size();
    }

    // метод is_empty(): перевіряє, чи буфер завдань порожній
    // повертає true, якщо буфер порожній
    bool is_empty() {
        return size() == 0;
    }

private:
    std::deque<Job> buffer; // внутрішній буфер для накопичення завдань, які очікують виконання
    std::mutex mtx; // м'ютекс для синхронізації доступу до буфера та прапорця ready
    std::shared_ptr<Channel<Job>> channel; // канал, куди будуть відправлятися завдання для виконання робітниками
    bool ready; // прапорець, що визначає, чи дозволено переносити завдання 
};

#endif // SCHEDULER_HPP
