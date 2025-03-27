#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class Channel {
public:
    Channel() = default;

    // метод для відправки елемента (тільки для lvalue)
    void send(const T& t) {
        std::unique_lock<std::mutex> lock(mtx);
        queue.push_back(t);
        cond.notify_one();
    }

    // метод для отримання елемента з блокуванням, поки черга порожня
    T receive() {
        std::unique_lock<std::mutex> lock(mtx);
        while (queue.empty()) {
            cond.wait(lock);
        }
        T val = queue.front();
        queue.pop_front();
        return val;
    }

    // метод перевірки, чи є черга порожньою
    bool is_empty() {
        std::unique_lock<std::mutex> lock(mtx);
        return queue.empty();
    }

private:
    std::deque<T> queue;
    std::mutex mtx;
    std::condition_variable cond;
};

#endif // CHANNEL_HPP
