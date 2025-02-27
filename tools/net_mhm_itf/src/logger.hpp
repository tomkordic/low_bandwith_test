#ifndef NETWORK_LOGGER_HPP
#define NETWORK_LOGGER_HPP

#include "constants.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <ctime>
#include <iomanip>

namespace networkinterface
{
    class Logger
    {
    public:
        enum class Severity
        {
            ERROR,
            INFO,
            VERBOSE,
            WARNING,
            DEBUG
        };

        // Get the singleton instance
        static Logger &getInstance()
        {
            static Logger instance;
            return instance;
        }

        void setLogLevel(Severity level)
        {
            logLevel = level;
        }

        // Set the log file
        void setLogFile(const std::string &filename)
        {
            std::lock_guard<std::mutex> lock(logMutex);
            if (logFile.is_open())
            {
                logFile.close();
            }
            logFile.open(filename, std::ios::out | std::ios::app);
            if (!logFile)
            {
                throw std::runtime_error("Failed to open log file.");
            }
        }

        // Log method that accepts std::ostringstream
        void log(const std::ostringstream &message, Severity severity = Severity::INFO, const std::string &tag = __FILE__)
        {
            log(message.str(), severity, tag); // Call the std::string version
        }

        // Log a message with severity and tag
        void log(const std::string &message, Severity severity = Severity::INFO, const std::string &tag = __FILE__)
        {
            std::lock_guard<std::mutex> lock(logMutex);
            if (logLevel < severity)
            {
                return;
            }
            std::ostringstream logEntry;

            // Add timestamp
            logEntry << getTimestamp() << " ";

            // Add severity
            logEntry << "[" << severityToString(severity) << "] ";

            // Add tag
            if (!tag.empty())
            {
                logEntry << "[" << tag << "] ";
            }

            // Add message
            logEntry << message;

            std::string STD_COLOR = RESET;
            switch (severity)
            {
            case Severity::DEBUG:
                STD_COLOR = YELLOW;
                break;
            case Severity::INFO:
                STD_COLOR = GREEN;
                break;
            case Severity::VERBOSE:
                STD_COLOR = BLUE;
                break;
            case Severity::WARNING:
                STD_COLOR = RESET;
                break;
            case Severity::ERROR:
                STD_COLOR = RED;
                break;
            }
            // Print to console
            std::cout << STD_COLOR << logEntry.str() << RESET << std::endl;

            // Write to log file if open
            if (logFile.is_open())
            {
                logFile << logEntry.str() << std::endl;
            }
        }

        // Disable copy constructor and assignment operator
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

    private:
        std::mutex logMutex;
        std::ofstream logFile;
        Severity logLevel = Severity::INFO;

        Logger() = default;
        ~Logger()
        {
            if (logFile.is_open())
            {
                logFile.close();
            }
        }

        // Convert severity enum to string
        std::string severityToString(Severity severity)
        {
            switch (severity)
            {
            case Severity::INFO:
                return "INFO";
            case Severity::WARNING:
                return "WARNING";
            case Severity::ERROR:
                return "ERROR";
            case Severity::DEBUG:
                return "DEBUG";
            default:
                return "UNKNOWN";
            }
        }

        // Get the current timestamp in YYYY-MM-DD HH:MM:SS format
        std::string getTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            auto nowTimeT = std::chrono::system_clock::to_time_t(now);
            auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now.time_since_epoch()) %
                         1000;

            std::ostringstream timestamp;
            timestamp << std::put_time(std::localtime(&nowTimeT), "%Y-%m-%d %H:%M:%S")
                      << "." << std::setw(3) << std::setfill('0') << nowMs.count();

            return timestamp.str();
        }
    };
} // namespace playback

#endif // NETWORK_LOGGER_HPP
