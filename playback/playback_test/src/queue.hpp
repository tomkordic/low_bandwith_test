#ifndef PLAYBACK_QUEUE_HPP
#define PLAYBACK_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

namespace playback
{

    template <typename T>
    class Queue
    {
    public:
        explicit Queue(size_t maxSize) : maxSize(maxSize)
        {
            if (maxSize == 0)
            {
                throw std::invalid_argument("Queue size must be greater than zero.");
            }
        }

        ~Queue() = default;

        void push(const T &item)
        {
            if (!item)
            {
                throw std::invalid_argument("Cannot push a null item.");
            }

            std::unique_lock<std::mutex> lock(queueMutex);

            // Wait until there's space in the queue
            queueCondition.wait(lock, [this]()
                                { return _queue.size() < maxSize; });

            // Push the item into the queue
            _queue.push(item);

            // Notify consumers
            lock.unlock();
            queueCondition.notify_all();
        }

        T pop()
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            // Wait until there's an item in the queue
            queueCondition.wait(lock, [this]()
                                { return !_queue.empty(); });

            // Get the item from the queue
            T item = _queue.front();
            _queue.pop();

            // Notify producers
            lock.unlock();
            queueCondition.notify_all();

            return item;
        }

        bool empty()
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            return _queue.empty();
        }

        size_t size()
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            return _queue.size();
        }

        size_t getMaxSize()
        {
            return maxSize;
        }

    private:
        std::queue<T> _queue;                   // The underlying queue
        std::mutex queueMutex;                  // Mutex for thread safety
        std::condition_variable queueCondition; // Condition variable for blocking
        size_t maxSize;                         // Maximum size of the queue
    };

} // namespace playback

#endif // PLAYBACK_QUEUE_HPP