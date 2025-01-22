#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <chrono>
#include <string>

namespace networkmonitor
{

    // Define your constants here
    const std::string RED = "\033[31m";
    const std::string BLUE = "\033[34m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string RESET = "\033[0m";

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

    inline std::string ip_to_hostname(std::string ip)
    {
        char hostname[NI_MAXHOST];
        // Structure to store the address
        struct sockaddr_in sa;
        sa.sin_family = AF_INET; // IPv4
        inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr));

        // Perform reverse DNS lookup
        int result = getnameinfo((struct sockaddr *)&sa, sizeof(sa), hostname, NI_MAXHOST, nullptr, 0, 0);
        if (result == 0)
        {
            return std::string(hostname);
        }
        return "Client";
    }
} // namespace networkmonitor

#endif // CONSTANTS_HPP