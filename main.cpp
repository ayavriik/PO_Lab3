#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include "threadpool.hpp"

const int SLEEP_TIME = 45; // умова варіант 3
const size_t WORKERS = 4; // умова варіант 3

void task(int id) {
    // випадкова затримка між 4 і 10 секундами
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(4, 10);
    int duration = dis(gen);
    std::cout << "[" << id << "] Sleeping for " << duration << " seconds" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(duration));
}

int main() {
    // створення пулу потоків з WORKERS (4) робітниками та інтервалом очікування (SLEEP_TIME) 45 секунд
    // пул використовуватиме створені Worker-и для виконання завдань
    ThreadPool pool(WORKERS, std::chrono::seconds(SLEEP_TIME));

    // вектор для збереження потоків, які розкладають завдання
    std::vector<std::thread> handles;

    // запуск циклу для розкладання завдань
    // усього створюється 40 потоків, кожен з яких розкладає 4 завдання
    for (int i = 0; i < 40; ++i) {
        
        // для кожного i створюється окремий потік, який: виводить повідомлення "Scheduling a task" для кожного завдання
        // викликає метод execute() пулу для додавання завдання
        // pа допомогою лямбди розкладаємо завдання, де id завдання обчислюється як i*2 + j
        handles.emplace_back([i, &pool]() {
            for (int j = 0; j < 4; ++j) {
                std::cout << "Scheduling a task" << std::endl;
                pool.execute([id = i * 2 + j]() {

                    // викликаємо функцію task(), яка симулює виконання завдання
                    task(id);
                });
            }
        });
        // затримка між розкладанням завдань
        std::this_thread::sleep_for(std::chrono::seconds(7));
    }

    // завершення всіх потоків
    for (auto &handle : handles) {
        if (handle.joinable())
            handle.join();
    }

    std::cout << "All tasks scheduled" << std::endl;
    pool.join();
    std::cout << "Executor joined" << std::endl;
    return 0;
}
