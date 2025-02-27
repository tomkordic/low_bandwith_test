#include "logger.hpp"

#include <iostream>
#include <iostream>
#include <cstring> 
#include <chrono>
#include <thread>
#include <random>

#include <sys/ioctl.h> 
#include <linux/if.h>
#include <linux/route.h>
#include <linux/if_tun.h>

#include <viface/viface.hpp>

using namespace networkinterface;

constexpr const char *ITF_TAG = "NET_MAYHAM";

// Function to generate a random delay between min and max milliseconds
int getRandomDelay(int min_ms, int max_ms) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min_ms, max_ms);
    return dis(gen);
}

void add_default_route(const std::string &gateway, const std::string &interface_name)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        Logger::getInstance().log("Socket creation failed", Logger::Severity::ERROR, ITF_TAG);
        throw std::runtime_error("Socket creation failed");
    }

    struct rtentry route;
    memset(&route, 0, sizeof(route));

    // Set the gateway
    struct sockaddr_in *gateway_addr = (struct sockaddr_in *)&route.rt_gateway;
    gateway_addr->sin_family = AF_INET;
    inet_pton(AF_INET, gateway.c_str(), &gateway_addr->sin_addr);

    // Set the destination to 0.0.0.0 (default route)
    struct sockaddr_in *dest_addr = (struct sockaddr_in *)&route.rt_dst;
    dest_addr->sin_family = AF_INET;
    dest_addr->sin_addr.s_addr = INADDR_ANY;

    // Set the netmask to 0.0.0.0
    struct sockaddr_in *netmask_addr = (struct sockaddr_in *)&route.rt_genmask;
    netmask_addr->sin_family = AF_INET;
    netmask_addr->sin_addr.s_addr = INADDR_ANY;

    // Specify the interface
    route.rt_dev = const_cast<char *>(interface_name.c_str());
    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_metric = 0;

    // Add the route
    int ret = ioctl(sock, SIOCADDRT, &route);
    if (ret < 0)
    {
        Logger::getInstance().log("ioctl(SIOCADDRT), err: " + std::to_string(ret) + ", " + strerror(errno), Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("ioctl(SIOCADDRT)");
    }

    Logger::getInstance().log("Default route via " + gateway + " added for interface " + interface_name, Logger::Severity::INFO, ITF_TAG);
    close(sock);
}

int main() {
    try {
        // Create a TAP interface
        viface::VIface iface("mhm0", true); // 'true' enables the TAP interface immediately

        // Configure the interface
        iface.up();
        iface.setIPv4("192.168.100.3");
        iface.setIPv4Netmask("255.255.255.0");
        add_default_route("192.168.100.1", "mhm0");

        std::cout << "TAP interface 'tap0' is up and running.\n";

        // Main loop for processing packets
        while (true) {
            std::vector<uint8_t> packet;

            // Receive a packet from the TAP interface
            packet = iface.receive();

            if (!packet.empty()) {
                // Apply a random delay to incoming packet processing
                int delay_ms = getRandomDelay(100, 500); // Random delay between 100ms and 500ms
                std::cout << "Received packet. Delaying by " << delay_ms << " ms.\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

                // Process the packet (e.g., just log it for demonstration)
                std::cout << "Processing incoming packet of size " << packet.size() << " bytes.\n";

                // Optionally modify the packet here (e.g., change contents, add headers)

                // Apply a random delay before sending the packet back out
                delay_ms = getRandomDelay(100, 500);
                std::cout << "Delaying outgoing packet by " << delay_ms << " ms.\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

                // Send the packet back out
                iface.send(packet);
                std::cout << "Sent packet of size " << packet.size() << " bytes.\n";
            }
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
