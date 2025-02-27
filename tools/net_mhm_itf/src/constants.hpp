#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <chrono>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>

namespace networkinterface
{
    static const int MAC_ADDR_LEN = 6;

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

    inline std::string mac_to_string(uint8_t *mac)
    {
        std::ostringstream msg;
        for (int i = 0; i < 6; ++i)
        {
            // Print each byte in hexadecimal format with leading zeros
            msg << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
            if (i < 5)
            {
                msg << ":"; // Add colon between bytes
            }
        }
        return msg.str();
    }

    inline uint8_t* mac_string_to_bytes(const std::string &mac)
    {
        uint8_t *macBytes = (uint8_t *)malloc(MAC_ADDR_LEN);
        std::istringstream stream(mac);
        std::string byteString;

        int index = 0;
        while (std::getline(stream, byteString, ':'))
        {
            uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
            macBytes[index] = byte;
            index ++;
        }
        return macBytes;
    }

    // Function to convert uint8_t* to a hex string
    inline std::string buffer_to_hex_string(const uint8_t *buffer, size_t length)
    {
        if (!buffer)
        {
            throw std::invalid_argument("Buffer is null.");
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0'); // Use hex formatting and zero-padding

        for (size_t i = 0; i < length; ++i)
        {
            oss << std::setw(2) << static_cast<int>(buffer[i]); // Convert each byte to hex
            if (i != length - 1)
            {
                oss << " "; // Add a space between bytes for readability
            }
        }

        return oss.str();
    }

    // Helper function to convert a single hex character to its integer value
    inline uint8_t hexCharToByte(char c, int pos)
    {
        switch (c)
        {
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        case 'A':
            return 10;
        case 'a':
            return 10;
        case 'B':
            return 11;
        case 'b':
            return 11;
        case 'C':
            return 12;
        case 'c':
            return 12;
        case 'D':
            return 13;
        case 'd':
            return 13;
        case 'E':
            return 14;
        case 'e':
            return 14;
        case 'F':
            return 15;
        case 'f':
            return 15;
        default:
            throw std::invalid_argument("Invalid hex character: " + std::to_string(c) + " at position: " + std::to_string(pos));
        }
    }

    // Main function: Convert hex string to bytes
    inline void hexStringToBytes(const std::string &hex, uint8_t *bytes, int &size)
    {
        if (hex.size() % 2 != 0)
        {
            throw std::invalid_argument("Hex string must have an even length.");
        }
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            uint8_t byte = (hexCharToByte(hex[i], i) << 4) | hexCharToByte(hex[i + 1], i + 1);
            bytes[i / 2] = byte;
        }
        size = hex.size() / 2;
    }

} // namespace networkmonitor

#endif // CONSTANTS_HPP