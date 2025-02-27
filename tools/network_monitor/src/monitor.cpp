#include "logger.hpp"
#include "packet_processor.hpp"

#include <pcap.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <netinet/ip.h> // For IP header structures
#include <arpa/inet.h>  // For inet_ntoa()

using namespace networkmonitor;

constexpr const char *MAIN_TAG = "MAIN_THREAD";

int main(int argc, char *argv[])
{
    Logger::getInstance().setLogFile("network.log");
    if (argc != 2)
    {
        Logger::getInstance().log("Usage: " + std::string(argv[0]) + " <interface>", Logger::Severity::INFO, MAIN_TAG);
        return 1;
    }

    char *dev = argv[1]; // Network interface to capture packets on
    PacketProcessor processor(dev);
    while (processor.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        processor.print("");
    }

    return 0;
}
