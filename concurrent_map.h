#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {

private:
    struct Bucket {
        std::map<Key, Value> dict;
        std::mutex v_mutex;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard_;
        Value& ref_to_value;

        Access(Bucket& bucket, const Key& key) :
            guard_(bucket.v_mutex),
            ref_to_value(bucket.dict[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count) : bucket_(bucket_count) {}

    Access operator[](const Key& key) {
        size_t index = static_cast<uint64_t>(key) % bucket_.size();
        return { bucket_[index], key };
    }

    void Erase(const Key& key) {
        size_t index = static_cast<uint64_t>(key) % bucket_.size();
        std::lock_guard<std::mutex> guard(bucket_.at(index).v_mutex);
        bucket_.erase(index);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (size_t i = 0; i < bucket_.size(); ++i) {
            std::lock_guard<std::mutex> guard(bucket_.at(i).v_mutex);
            result.insert(bucket_.at(i).dict.begin(), bucket_.at(i).dict.end());
        }
        return result;
    }
private:
    std::vector<Bucket> bucket_;
};

template <typename Value>
class ConcurrentSet {

private:
    struct Bucket {
        std::set<Value> dict;
        std::mutex v_mutex;
    };

public:
    explicit ConcurrentSet(size_t bucket_count) : bucket_(bucket_count) {}
    
    void Insert(Value value) {
        size_t index = static_cast<uint64_t>(value) % bucket_.size();
        std::lock_guard<std::mutex> guard(bucket_.at(index).v_mutex);
        bucket_.at(index).dict.insert(value);
    }

    bool Contains(Value value) {
        size_t index = static_cast<uint64_t>(value) % bucket_.size();
        return bucket_.at(index).dict.count(value)>0;
    }

    std::set<Value> BuildOrdinaryMap() {
        std::set<Value> result;
        for (size_t i = 0; i < bucket_.size(); ++i) {
            std::lock_guard<std::mutex> guard(bucket_.at(i).v_mutex);
            result.insert(bucket_.at(i).dict.begin(), bucket_.at(i).dict.end());
        }
        return result;
    }
private:
    std::vector<Bucket> bucket_;
};