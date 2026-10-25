// The Flash game's score and time texts (_root.formatScore, formatTime).
#pragma once

#include <cmath>
#include <string>

namespace xmas {

// 12345 -> "12,345".
inline std::string formatScore(double score) {
    std::string digits = std::to_string(static_cast<long long>(score));
    std::string out;
    int n = 0;
    for (size_t i = digits.size(); i-- > 0;) {
        out.insert(out.begin(), digits[i]);
        if (++n % 3 == 0 && i > 0 && digits[i - 1] != '-') out.insert(out.begin(), ',');
    }
    return out;
}

// Movie frames (25 a second) as "m:ss".
inline std::string formatTime(double frames) {
    double ms = frames / 25 * 1000;
    long minutes = static_cast<long>(std::floor(ms / 60000));
    long seconds = static_cast<long>(std::floor((ms - minutes * 60000) / 1000));
    return std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);
}

}  // namespace xmas
