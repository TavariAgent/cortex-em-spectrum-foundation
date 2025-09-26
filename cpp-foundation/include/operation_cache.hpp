#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <chrono>
#include <optional>
#include <functional>

namespace cortex {

    struct OpCacheEntry {
        std::chrono::steady_clock::time_point ts;
        size_t bytes;
        // You can template this; for now keep value opaque as std::shared_ptr<void>
        std::shared_ptr<void> value;
    };

    class OperationCache {
    public:
        explicit OperationCache(size_t max_bytes = 64 * 1024 * 1024)
            : max_bytes_(max_bytes) {}

        template<typename T>
        void put(const std::string& key, std::shared_ptr<T> v, size_t bytes_hint=0) {
            if (!v) return;
            size_t sz = bytes_hint ? bytes_hint : sizeof(T);
            {
                std::unique_lock lk(mu_);
                entries_[key] = OpCacheEntry{ std::chrono::steady_clock::now(), sz, v };
                used_bytes_ += sz;
            }
            evict_if_needed();
        }

        template<typename T>
        std::shared_ptr<T> get(const std::string& key) const {
            std::shared_lock lk(mu_);
            auto it = entries_.find(key);
            if (it == entries_.end()) return {};
            return std::static_pointer_cast<T>(it->second.value);
        }

        void set_max_bytes(size_t b) { max_bytes_ = b; }

    private:
        void evict_if_needed() {
            std::unique_lock lk(mu_);
            if (used_bytes_ <= max_bytes_) return;

            // Simple oldest-first purge
            std::vector<std::pair<std::string, OpCacheEntry>> v(entries_.begin(), entries_.end());
            std::sort(v.begin(), v.end(), [](auto& a, auto& b){
                return a.second.ts < b.second.ts;
            });
            for (auto& kv : v) {
                if (used_bytes_ <= max_bytes_) break;
                used_bytes_ -= kv.second.bytes;
                entries_.erase(kv.first);
            }
        }

        mutable std::shared_mutex mu_;
        std::unordered_map<std::string, OpCacheEntry> entries_;
        size_t used_bytes_{0};
        size_t max_bytes_;
    };

} // namespace cortex