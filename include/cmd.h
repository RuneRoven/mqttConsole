#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
namespace cmd {
    struct Command {
        std::string topic;
        std::string subtopic;
        std::string message;
        // Add other fields as needed
    };
    class CommandQueue {
    private:
        std::queue<Command> queue;
        std::mutex mtx;
        std::condition_variable cv;
    public:
        void push(const Command& cmd) {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(cmd);
            cv.notify_one(); // Notify the MQTT thread
        }

        Command pop() {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return !queue.empty(); }); // Wait for a command
            Command cmd = queue.front();
            queue.pop();
            return cmd;
        }
        bool try_pop(Command& cmd) {
            std::lock_guard<std::mutex> lock(mtx);
            if (queue.empty()) return false;
            cmd = queue.front();
            queue.pop();
            return true;
        }
    };
}