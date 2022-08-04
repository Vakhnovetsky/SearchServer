#pragma once

#include <chrono>
#include <iostream>
#include <string_view>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION_STREAM(x, s) LogDuration UNIQUE_VAR_NAME_PROFILE(x, s)

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string_view& id, std::ostream& stream)
        : id_(id), stream_(stream) {
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        stream_ << id_.substr() << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }

private:
    const std::string_view id_;
    std::ostream& stream_= std::cerr;
    const Clock::time_point start_time_ = Clock::now();
};