#pragma once
#include <cstddef>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace novadb {

struct Options {
    std::size_t write_buffer_size = 4 * 1024 * 1024;
    bool create_if_missing = true;
};

struct WriteOperation {
    enum class Type : unsigned char { Put = 1, Delete = 2 };
    Type type;
    std::string key;
    std::string value;
};

class WriteBatch {
public:
    WriteBatch& put(std::string key, std::string value);
    WriteBatch& erase(std::string key);
    const std::vector<WriteOperation>& operations() const noexcept { return operations_; }
    bool empty() const noexcept { return operations_.empty(); }
    void clear() noexcept { operations_.clear(); }
private:
    std::vector<WriteOperation> operations_;
};

class DB {
public:
    static DB open(const std::filesystem::path& path, Options options = {});
    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;
    DB(DB&&) = delete;
    DB& operator=(DB&&) = delete;
    ~DB();

    void put(const std::string& key, const std::string& value);
    bool erase(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;
    void write(const WriteBatch& batch);
    std::vector<std::pair<std::string, std::string>> scan() const;
    void flush();
    std::size_t size() const;
    std::uint64_t last_sequence() const;

private:
    DB(std::filesystem::path path, Options options);
    void recover();
    void append_wal(const WriteOperation& op, std::uint64_t sequence);
    void apply(const WriteOperation& op, std::uint64_t sequence);
    void flush_locked();

    std::filesystem::path path_;
    Options options_;
    mutable std::mutex mutex_;
    std::map<std::string, std::pair<std::uint64_t, std::optional<std::string>>> memtable_;
    std::uint64_t sequence_ = 0;
    std::uint64_t generation_ = 0;
    std::size_t approximate_bytes_ = 0;
    bool closed_ = false;
};

} // namespace novadb
