#pragma once
#include <unordered_map>
#include <string>
#include <mutex>

class SharedData {
public:
    void update(const std::string& topic, const std::string& payload) {
        std::lock_guard<std::mutex> lock(write_mtx_);
        write_buf_[topic] = payload;
        dirty_ = true;
    }

    void swap_buffers() {
        if (!dirty_) return;
        std::lock_guard<std::mutex> lock(write_mtx_);
        for (auto& [topic, payload] : write_buf_) {
            read_buf_[topic] = payload; // merge or update existing topic
        }
        write_buf_.clear();
        dirty_ = false;
    }


    const std::unordered_map<std::string, std::string>& read() const {
        return read_buf_;
    }

private:
    std::unordered_map<std::string, std::string> read_buf_;
    std::unordered_map<std::string, std::string> write_buf_;
    std::mutex write_mtx_;
    bool dirty_ = false;
};
