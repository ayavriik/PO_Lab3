#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class Channel {
public:
    Channel() = default;

    // перевантаження send для lvalue
    void send(const T& t) {
        std::unique_lock<std::mutex> lock(mtx);
        queue.push_back(t);
        cond.notify_one();
    }

    // перевантаження send для rvalue
    void send(T&& t) {
        std::unique_lock<std::mutex> lock(mtx);
        queue.push_back(std::move(t));
        cond.notify_one();
    }

    // метод отримання елемента з блокуванням, поки черга порожня
    T receive() {
        std::unique_lock<std::mutex> lock(mtx);
        while (queue.empty()) {
            cond.wait(lock);
        }
        T val = std::move(queue.front());
        queue.pop_front();
        return val;
    }

    // метод перевірки, чи є черга порожньою
    bool is_empty() {
        std::unique_lock<std::mutex> lock(mtx);
        return queue.empty();
    }

    // метод отримання поточного розміру черги
    size_t len() {
        std::unique_lock<std::mutex> lock(mtx);
        return queue.size();
    }

private:
    std::deque<T> queue;
    std::mutex mtx;
    std::condition_variable cond;
};

#endif // CHANNEL_HPP
