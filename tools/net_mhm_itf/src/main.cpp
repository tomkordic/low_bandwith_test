#include "logger.hpp"
#include "interface.hpp"


#include <iostream>
#include <unordered_map>
#include <getopt.h>  // For parsing command-line options
#include <fcntl.h>
#include <thread>

using namespace networkinterface;

constexpr const char *MAIN_TAG = "MAIN_THREAD";


// Default values
std::string device_name = "mhm0";
std::string ip_address = "192.168.100.10";
std::string netmask = "255.255.255.0";
std::string mac_address = "";
std::string bridge_name = "br0";
std::string gateway = "192.168.100.1";

// Function to display help message
void print_help(const std::string& program_name) {
    Logger::getInstance().log("Usage: " + program_name + " [options]\n\n"
                              "Options:\n"
                              "  -b, --bridge <name>     Bridge name (default: " + bridge_name + ")\n"
                              "  -d, --device <name>     Device name (default: " + device_name + ")\n"
                              "  -i, --ip <address>      IP address (default: " + ip_address + ")\n"
                              "  -m, --mac <address>     MAC address (default: " + mac_address + ")\n"
                              "  -n, --netmask <mask>    Netmask (default: " + netmask + ")\n"
                              "  -g, --gateway <address> Gateway address (default: " + gateway + ")\n"
                              "  -h, --help              Display this help message",
                              Logger::Severity::INFO, MAIN_TAG);
}

// Function to parse and validate command-line arguments
void parse_arguments(int argc, char* argv[]) {
    const char* const short_opts = "d:i:m:g:h";
    const option long_opts[] = {
        {"bridge",  required_argument, nullptr, 'b'},
        {"device",  required_argument, nullptr, 'd'},
        {"ip",      required_argument, nullptr, 'i'},
        {"mac",     required_argument, nullptr, 'm'},
        {"gateway", required_argument, nullptr, 'g'},
        {"help",    no_argument,       nullptr, 'h'},
        {nullptr,   0,                 nullptr,  0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'b':
                bridge_name = optarg;
                break;
            case 'd':
                device_name = optarg;
                break;
            case 'i':
                ip_address = optarg;
                break;
            case 'm':
                mac_address = optarg;
                break;
            case 'g':
                gateway = optarg;
                break;
            case 'h':
                print_help(argv[0]);
                exit(0);
            default:
                print_help(argv[0]);
                exit(1);
        }
    }
}

int main(int argc, char *argv[])
{
    Logger::getInstance().setLogFile("interface.log");
    parse_arguments(argc, argv);
    Logger::getInstance().setLogLevel(Logger::Severity::VERBOSE);

    // Set stdin (fd 0) to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    Logger::getInstance().log("Non-blocking read from stdin.", Logger::Severity::INFO, MAIN_TAG);

    MayhemInterface itf(device_name, ip_address, netmask, mac_address, gateway, bridge_name);
    while (itf.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        itf.print("");
    }

    return 0;
}
