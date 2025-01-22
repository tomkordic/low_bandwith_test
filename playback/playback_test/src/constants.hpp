#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

extern "C"
{
#include <libavutil/avutil.h>
}

#include <iostream>
#include <regex>
#include <chrono>

namespace playback
{

    // Define your constants here
    const std::string RED = "\033[31m";
    const std::string BLUE = "\033[34m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string RESET = "\033[0m";

    const static std::regex decimal_regex(R"(\b\d+\.\d+\b)");
    const static std::regex url_regex(R"((http[s]?:\/\/[^\s']+))");
    const static std::regex numberRegex(R"(-(\d+)\.ts$)"); // Matches '-<number>.ts' at the end of the string

    /**
     * @brief Get the current UTC time in milliseconds since the epoch.
     *
     * @return long The current UTC time in milliseconds.
     */
    inline long get_utc()
    {
        auto now = std::chrono::system_clock::now();
        // Get duration since epoch in milliseconds
        auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        // Return as long
        return static_cast<long>(ms_since_epoch);
    }

    inline double pts_to_ms(AVFrame *frame, AVRational time_base)
    {
        if (time_base.num == 0)
        {
            return frame->pts;
        }
        return ((double)(frame->pts * 1000.0 * time_base.num) / (double)time_base.den);
    }

    inline int extract_sequence_number(std::string uri)
    {
        std::smatch match;
        if (std::regex_search(uri, match, numberRegex))
        {
            if (match.size() > 1)
            { // The first capture group is the number
                std::string number = match[1];
                return std::stoi(number);
            }
        }
        else
        {
            throw std::runtime_error("Failed to find sequence number in segment uri: " + uri);
        }
        return -1;
    }
} // namespace playback

#endif // CONSTANTS_HPP