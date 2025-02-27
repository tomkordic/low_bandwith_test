#include "interface.hpp"
#include "logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
// #include <linux/if.h>
#include <net/if.h>
#include <linux/if_arp.h>
#include <linux/route.h>
#include <linux/if_tun.h>
#include <linux/if_bridge.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstream>
#include <vector>
#include <stdexcept>

// Define SIOCBRADDIF if missing
#ifndef SIOCBRADDIF
#define SIOCBRADDIF 0x89a2
#endif

using namespace networkinterface;

constexpr const char *ITF_TAG = "NET_MAYHAM";

MayhemInterface::MayhemInterface(std::string dev, std::string ip, std::string netmask, std::string mac, std::string gateway, std::string bridge)
    : dev(dev), local_ip(ip), bridge(bridge), mac(mac), fd(-1), stop_running(false),
      total_received_bytes(0), total_sent_bytes(0), received_bytes_per_period(0), sent_bytes_per_period(0),
      received_packets_per_period(0), sent_packets_per_period(0),
      last_print(get_utc()), started(get_utc())
{
    Logger::getInstance().log("\n\n ======= Meyham<" + dev + "> ======\n\n", Logger::Severity::INFO, ITF_TAG);
    createInterface();
    if (mac.size() > 0)
    {
        set_mac_address();
    } else {
        get_mac();
    }
    Logger::getInstance().log("just to check, mac: " + this->mac, Logger::Severity::INFO, ITF_TAG);
    add_to_bridge();
    // Bring up the interface
    bring_up_interface();
    // Add an IP address
    add_ip_address(ip, netmask);

    // Add a default route
    add_default_route(gateway);
    worker = std::thread(&MayhemInterface::run, this);
}

MayhemInterface::~MayhemInterface()
{
    // Stop running and wait for the thread to finish
    stop_running = true;
    if (worker.joinable())
    {
        worker.join();
    }
}

void MayhemInterface::createInterface()
{
    struct ifreq ifr;
    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0)
    {
        Logger::getInstance().log("Failed to open /dev/net/tun", Logger::Severity::ERROR, ITF_TAG);
        throw std::runtime_error("Failed to open /dev/net/tun");
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI; // IFF_TAP for TAP, IFF_NO_PI to exclude packet info
    strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0)
    {
        Logger::getInstance().log("ioctl(TUNSETIFF)", Logger::Severity::ERROR, ITF_TAG);
        close(fd);
        throw std::runtime_error("ioctl(TUNSETIFF)");
    }
    Logger::getInstance().log("Created interface: " + std::string(ifr.ifr_name), Logger::Severity::INFO, ITF_TAG);

    // Disable IPv6 on the TAP interface using sysctl
    std::string ipv6_sysctl_path = "/proc/sys/net/ipv6/conf/" + std::string(ifr.ifr_name) + "/disable_ipv6";
    std::ofstream sysctl_file(ipv6_sysctl_path);
    if (sysctl_file)
    {
        sysctl_file << "1"; // Disable IPv6
        sysctl_file.close();
        Logger::getInstance().log("Disabled IPv6 on interface: " + std::string(ifr.ifr_name), Logger::Severity::INFO, ITF_TAG);
    }
    else
    {
        Logger::getInstance().log("Failed to disable IPv6 on interface: " + std::string(ifr.ifr_name), Logger::Severity::WARNING, ITF_TAG);
    }
}

void MayhemInterface::get_mac() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        Logger::getInstance().log("Failed to open socket", Logger::Severity::ERROR, ITF_TAG);
        throw std::runtime_error("Failed to open socket");
    }

    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        Logger::getInstance().log("ioctl error", Logger::Severity::ERROR, ITF_TAG);
        close(fd);
        throw std::runtime_error("ioctl error");
    }
    close(fd);

    unsigned char *mac = reinterpret_cast<unsigned char *>(ifr.ifr_hwaddr.sa_data);
    char macAddr[18];
    std::snprintf(macAddr, sizeof(macAddr),
                  "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    this->mac = std::string(macAddr);
    Logger::getInstance().log(dev + " uses mac: " + this->mac, Logger::Severity::INFO, ITF_TAG);
}

void MayhemInterface::add_to_bridge()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        Logger::getInstance().log("Failed to open socket", Logger::Severity::ERROR, ITF_TAG);
        throw std::runtime_error("Failed to open socket");
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, bridge.c_str(), IFNAMSIZ);

    // Retrieve the index of the interface to be added
    ifr.ifr_ifindex = if_nametoindex(dev.c_str());
    if (ifr.ifr_ifindex == 0)
    {
        Logger::getInstance().log("Error: Interface " + dev + " does not exist.", Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("Error: Interface " + dev + " does not exist.");
    }

    // Perform the ioctl to add the interface to the bridge
    if (ioctl(sock, SIOCBRADDIF, &ifr) < 0)
    {
        Logger::getInstance().log("Failed to add interface " + dev + " to bridge " + bridge, Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("Failed to add interface " + dev + " to bridge " + bridge);
    }

    Logger::getInstance().log("Successfully added " + dev + " to bridge " + bridge, Logger::Severity::INFO, ITF_TAG);
    close(sock);
}

void MayhemInterface::set_mac_address()
{
    int sock;
    struct ifreq ifr;

    // Create a socket for ioctl operations
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        throw std::runtime_error("Socket creation failed");
    }

    // Copy the interface name to the ifreq structure
    strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ);

    // Convert MAC address string to bytes
    unsigned char mac_bytes[6];
    std::istringstream mac_stream(mac);
    std::string byte;
    int i = 0;

    while (std::getline(mac_stream, byte, ':') && i < 6)
    {
        mac_bytes[i] = static_cast<unsigned char>(std::stoi(byte, nullptr, 16));
        i++;
    }

    if (i != 6)
    {
        close(sock);
        throw std::runtime_error("Invalid MAC address format");
    }

    // Set the MAC address in the ifreq structure
    memcpy(ifr.ifr_hwaddr.sa_data, mac_bytes, 6);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    // Perform the ioctl call to set the MAC address
    if (ioctl(sock, SIOCSIFHWADDR, &ifr) < 0)
    {
        close(sock);
        throw std::runtime_error("Failed to set MAC address");
    }

    close(sock);
    Logger::getInstance().log("MAC address changed successfully on interface " + dev, Logger::Severity::INFO, ITF_TAG);
}

void MayhemInterface::add_default_route(const std::string &gateway)
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
    route.rt_dev = const_cast<char *>(dev.c_str());
    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_metric = 0;

    // Add the route
    if (ioctl(sock, SIOCADDRT, &route) < 0)
    {
        Logger::getInstance().log("ioctl(SIOCADDRT)", Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("ioctl(SIOCADDRT)");
    }

    Logger::getInstance().log("Default route via " + gateway + " added for interface " + dev, Logger::Severity::INFO, ITF_TAG);
    close(sock);
}

void MayhemInterface::add_ip_address(const std::string &ip_address, const std::string &netmask)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        Logger::getInstance().log("Socket creation failed", Logger::Severity::INFO, ITF_TAG);
        throw std::runtime_error("Socket creation failed");
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    // Set the IP address
    inet_pton(AF_INET, ip_address.c_str(), &addr.sin_addr);
    memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
    if (ioctl(sock, SIOCSIFADDR, &ifr) < 0)
    {
        Logger::getInstance().log("ioctl(SIOCSIFADDR)", Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("ioctl(SIOCSIFADDR)");
    }

    // Set the netmask
    inet_pton(AF_INET, netmask.c_str(), &addr.sin_addr);
    memcpy(&ifr.ifr_netmask, &addr, sizeof(addr));
    if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0)
    {
        Logger::getInstance().log("ioctl(SIOCSIFNETMASK)", Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("ioctl(SIOCSIFNETMASK)");
    }

    Logger::getInstance().log("IP address " + ip_address + " with netmask " + netmask + " added to " + dev, Logger::Severity::INFO, ITF_TAG);
    close(sock);
}

void MayhemInterface::bring_up_interface()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        Logger::getInstance().log("Socket creation failed", Logger::Severity::INFO, ITF_TAG);
        throw std::runtime_error("Socket creation failed");
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ);

    // Get current flags
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
    {
        Logger::getInstance().log("ioctl(SIOCGIFFLAGS)", Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("ioctl(SIOCGIFFLAGS)");
    }

    // Set the IFF_UP flag to bring up the interface
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0)
    {
        Logger::getInstance().log("ioctl(SIOCSIFFLAGS)", Logger::Severity::ERROR, ITF_TAG);
        close(sock);
        throw std::runtime_error("ioctl(SIOCSIFFLAGS)");
    }

    Logger::getInstance().log("Interface " + dev + " is up.", Logger::Severity::INFO, ITF_TAG);
    close(sock);
}

void MayhemInterface::set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0); // Get current flags
    if (flags == -1)
    {
        Logger::getInstance().log("fcntl(F_GETFL)" + std::to_string(errno), Logger::Severity::ERROR, ITF_TAG);
        throw std::runtime_error("fcntl(F_GETFL)");
    }

    // Set the O_NONBLOCK flag
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        Logger::getInstance().log("fcntl(F_SETFL)" + std::to_string(errno), Logger::Severity::ERROR, ITF_TAG);
        throw std::runtime_error("fcntl(F_SETFL)");
    }
    else
    {
        Logger::getInstance().log("File descriptor " + std::to_string(fd) + " set to non-blocking mode", Logger::Severity::INFO, ITF_TAG);
    }
}

void MayhemInterface::write_packet(std::shared_ptr<EthernetPacket> packet)
{
    Logger::getInstance().log("writing packet, i: " + std::to_string(packet->index) + ", HEX: " + packet->parser->hex(0, packet->parser->_bytes_written), Logger::Severity::INFO, ITF_TAG);
    int nwrite = write(fd, packet->parser->_storage, packet->parser->_bytes_written);
    if (nwrite != packet->parser->_bytes_written)
    {
        Logger::getInstance().log("Failed to write packet, w: " + std::to_string(nwrite) + ", l: " + std::to_string(packet->parser->_bytes_written) + ", i: " + std::to_string(packet->index), Logger::Severity::ERROR, ITF_TAG);
    }
}

void MayhemInterface::read_packet(std::shared_ptr<EthernetPacket> packet, std::shared_ptr<BufferMixin> parser)
{
    packet->eth_header.parse(parser);
    switch (packet->eth_header.ethertype)
    {
    case EthernetType::IPv4:
        Logger::getInstance().log("PR: IPv4, i: " + std::to_string(packet->index), Logger::Severity::DEBUG, ITF_TAG);
        // read_ipv4(packet, parser);
        break;
    case EthernetType::IPv6:
        Logger::getInstance().log("PR: IPv6, i: " + std::to_string(packet->index), Logger::Severity::DEBUG, ITF_TAG);
        packet->ipv6_header.parse(parser);
        if (packet->ipv6_header.payload_length != parser->bytes_left())
        {
            throw std::runtime_error("Remaining data on parser after IPv6 header do not match the packet payload, r: " + std::to_string(parser->bytes_left()) + ", pl: " + std::to_string(packet->ipv6_header.payload_length));
        }
        // Logger::getInstance().log("IPv6 (hex): " + parser->hex(0, parser->_bytes_written), Logger::Severity::INFO, ITF_TAG);
        break;
    case EthernetType::ARP:
        Logger::getInstance().log("PR: ARP, i: " + std::to_string(packet->index), Logger::Severity::DEBUG, ITF_TAG);
        packet->arp_header.parse(parser);
        if (parser->bytes_left() > 0)
        {
            throw std::runtime_error("Remaining data on parser after ARP header, remaining: " + std::to_string(parser->bytes_left()));
        }
        break;
    default:
        throw std::runtime_error("Unsupported protocol: " + std::to_string((int)packet->eth_header.ethertype));
    }

    packet->parser = parser;
    packet->read_done = true;
}

EthernetPacket::EthernetPacket(int index) : index(index)
{
}

EthernetPacket::~EthernetPacket()
{
    Logger::getInstance().log("Releasing packet " + std::to_string(index), Logger::Severity::DEBUG, ITF_TAG);
}

std::string EthernetPacket::get_src_ip()
{
    switch (eth_header.ethertype)
    {
    case EthernetType::IPv4:
    {
        // struct in_addr src_addr;
        // src_addr.s_addr = piv4;
        // return inet_ntoa(src_addr);
    }
    case EthernetType::IPv6:
    {
        char src_addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, ipv6_header.source_ip, src_addr, INET6_ADDRSTRLEN);
        return src_addr;
    }
    case EthernetType::ARP:
    {
        char src_ip[100];
        if (arp_header.protocol_addr_len == 4)
        {
            if (!inet_ntop(AF_INET, arp_header.sender_ip, src_ip, INET_ADDRSTRLEN))
            {
                Logger::getInstance().log("Failed to convert ipv4 ip, errno: " + std::to_string(errno), Logger::Severity::ERROR, ITF_TAG);
            }
        }
        else if (arp_header.protocol_addr_len == 16)
        {
            if (!inet_ntop(AF_INET6, arp_header.sender_ip, src_ip, INET6_ADDRSTRLEN))
            {
                Logger::getInstance().log("Failed to convert ipv6 ip, errno: " + std::to_string(errno), Logger::Severity::ERROR, ITF_TAG);
            }
        }
        else
        {
            throw std::runtime_error("Unsupported adress length in ARP packet, al: " + std::to_string(arp_header.protocol_addr_len));
        }
        return std::string(src_ip);
    }
    default:
        throw std::runtime_error("Unsupported protocol: " + std::to_string((int)eth_header.ethertype));
    }
}

std::string EthernetPacket::get_dst_ip()
{
    switch (eth_header.ethertype)
    {
    case EthernetType::IPv4:
    {
        // struct in_addr dst_addr;
        // dst_addr.s_addr = ip_header->daddr;
        // return inet_ntoa(dst_addr);
    }
    case EthernetType::IPv6:
    {
        char dst_addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, ipv6_header.dest_ip, dst_addr, INET6_ADDRSTRLEN);
        return dst_addr;
    }
    case EthernetType::ARP:
    {
        char dest_ip[100];
        if (arp_header.protocol_addr_len == 4)
        {
            inet_ntop(AF_INET, arp_header.target_ip, dest_ip, INET_ADDRSTRLEN);
        }
        else if (arp_header.protocol_addr_len == 16)
        {
            inet_ntop(AF_INET6, arp_header.target_ip, dest_ip, INET6_ADDRSTRLEN);
        }
        else
        {
            throw std::runtime_error("Unsupported adress length in ARP packet, al: " + std::to_string(arp_header.protocol_addr_len));
        }
        return dest_ip;
    }
    default:
        throw std::runtime_error("Unsupported protocol: " + std::to_string((int)eth_header.ethertype));
    }
}

void EthernetPacket::print(std::string prefix)
{
    Logger::getInstance().log(prefix + "===== " + eth_header.ethernet_type_to_str() + " Packet =====", Logger::Severity::INFO, ITF_TAG);
    Logger::getInstance().log(prefix + "Dest MAC: " + mac_to_string(eth_header.dest_mac), Logger::Severity::INFO, ITF_TAG);
    Logger::getInstance().log(prefix + "Src MAC: " + mac_to_string(eth_header.src_mac), Logger::Severity::INFO, ITF_TAG);
    Logger::getInstance().log(prefix + "Dest IP: " + get_dst_ip(), Logger::Severity::INFO, ITF_TAG);
    Logger::getInstance().log(prefix + "Src IP: " + get_src_ip(), Logger::Severity::INFO, ITF_TAG);
    if (eth_header.ethertype == EthernetType::ARP)
    {
        Logger::getInstance().log(prefix + "Hex: " + parser->hex(0, parser->_position), Logger::Severity::INFO, ITF_TAG);
    }
}

void MayhemInterface::run()
{
    char stdin_buffer[3000];
    int sdtin_buff_position = 0;
    int packet_index = 0;
    set_non_blocking(fd);
    std::shared_ptr<EthernetPacket> packet = std::make_shared<EthernetPacket>(packet_index);
    while (!stop_running)
    {

        // Read a packet from the TUN interface
        try
        {
            std::shared_ptr<BufferMixin> parser = BufferMixin::from_file_descripto(fd, ETHERNET_PACKET_MAX_SIZE);
            read_packet(packet, parser);
        }
        catch (const NoDataException &exc)
        {
            usleep(30000); // Sleep for 30ms to avoid busy-waiting
            process_stdin(stdin_buffer, sdtin_buff_position, sizeof(stdin_buffer));
            continue;
        }
        catch (const std::runtime_error &err)
        {
            Logger::getInstance().log("Runtime error: " + std::string(err.what()), Logger::Severity::ERROR, ITF_TAG);
            throw err;
        }
        packet->print("");
        if (packet->eth_header.ethertype == EthernetType::ARP)
        {
            std::string src_mac = mac_to_string(packet->eth_header.src_mac);
            // TODO: do a broadcast check in a better way.
            if (mac_to_string(packet->eth_header.dest_mac) == "ff:ff:ff:ff:ff:ff")
            {
                // it is a broadcast
                Logger::getInstance().log("Processing ARP broadcast", Logger::Severity::VERBOSE, ITF_TAG);
                if ((packet->get_dst_ip().compare(local_ip) == 0) && (packet->arp_header.operation == (int)ARPOperationType::REQUEST))
                {
                    // TODO: let the kernel formulate the rply, expect a reply to be comming to fd, and just write it back to the FD.
                    // Formulate a ARP reply
                    Logger::getInstance().log("ARP request received for my machine", Logger::Severity::VERBOSE, ITF_TAG);
                    std::shared_ptr<EthernetPacket> response = clone_packet(packet);
                    uint8_t *mac_bytes = mac_string_to_bytes(mac);
                    response->arp_header.set_src_mac(mac_bytes, response->parser);
                    free(mac_bytes);
                    response->arp_header.set_dest_mac(packet->arp_header.sender_mac, response->parser);
                    // Logger::getInstance().log("Preaparing ARP response operations field", Logger::Severity::VERBOSE, ITF_TAG);
                    response->arp_header.set_operation(ArpOperations::ARP_REPLY, response->parser);
                    Logger::getInstance().log("ARP response, Hex: " + response->parser->hex(0, response->parser->_position), Logger::Severity::VERBOSE, ITF_TAG);
                    write_packet(response);
                }
                else
                {
                    Logger::getInstance().log("Considering retransmitting ARP broadcast", Logger::Severity::VERBOSE, ITF_TAG);
                    if (broadcast_timetable.find(src_mac) != broadcast_timetable.end()) {
                        long last_broadcast = broadcast_timetable[src_mac];
                        if (get_utc() - last_broadcast > REBROADCAST_INTERVAL) {
                            Logger::getInstance().log("Resending ARP broadcast from timetable", Logger::Severity::VERBOSE, ITF_TAG);
                            // write broadcast, it will loop back, so save it to map to ignore it next time
                            broadcast_timetable[src_mac] = get_utc();
                            write_packet(packet);    
                        } else {
                            Logger::getInstance().log("ARP broadcast already sent", Logger::Severity::VERBOSE, ITF_TAG);
                        }
                    } else {
                        Logger::getInstance().log("Resending ARP broadcast", Logger::Severity::VERBOSE, ITF_TAG);
                        // write broadcast, it will loop back, so save it to map to ignore it next time
                        broadcast_timetable[src_mac] = get_utc();
                        write_packet(packet);
                    }
                }
            }
        }
        // write_packet(packet);
        packet_index++;
        packet = std::make_shared<EthernetPacket>(packet_index);
    }
}

void MayhemInterface::process_stdin(char *buffer, int &position, int buffer_size)
{
    // Logger::getInstance().log("Try stdin: p: " + std::to_string(position) + ", bs: " + std::to_string(buffer_size), Logger::Severity::INFO, "STDIN");
    ssize_t bytesRead = read(STDIN_FILENO, buffer + position, buffer_size - (1 + position));
    if (bytesRead > 0)
    {
        Logger::getInstance().log("Got bytes: " + std::to_string(bytesRead), Logger::Severity::INFO, "STDIN");
        buffer[bytesRead + position] = '\0';
        position += bytesRead;
        if (buffer[position - 1] == '\n')
        {
            buffer[position - 1] = '\0';
            Logger::getInstance().log("Got packet, l: " + std::to_string(position), Logger::Severity::INFO, "STDIN");
            uint8_t packet[1500];
            int size = 0;
            try
            {
                hexStringToBytes(std::string(buffer), packet, size);
                int nwrite = write(fd, packet, size);
                if (nwrite != size)
                {
                    Logger::getInstance().log("Failed to write stdin data to TAP", Logger::Severity::ERROR, ITF_TAG);
                }
                else
                {
                    Logger::getInstance().log("STDIN packet sent, size: " + std::to_string(size), Logger::Severity::INFO, ITF_TAG);
                }
            }
            catch (const std::invalid_argument &e)
            {
                Logger::getInstance().log("Failed to parse input hex, error: " + std::string(e.what()), Logger::Severity::ERROR, ITF_TAG);
            }
            position = 0;
        }
    }
}

std::shared_ptr<EthernetPacket> MayhemInterface::clone_packet(std::shared_ptr<EthernetPacket> original) {
    std::shared_ptr<BufferMixin> cloned_parser = original->parser->clone();
    std::shared_ptr<EthernetPacket> clone = std::make_shared<EthernetPacket>(original->index);
    read_packet(clone, cloned_parser);
    return clone;
}

void MayhemInterface::print(std::string prefix)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    // long period = get_utc() - last_print;
    // long up_speed = sent_bytes_per_period / (period);       // kbps
    // long down_speed = received_bytes_per_period / (period); // kbps
    // long average_up_speed = total_sent_bytes / ((get_utc() - started));
    // long average_down_speed = total_received_bytes / ((get_utc() - started));
    // Logger::getInstance().log(prefix + "=====  NET STATS =====", Logger::Severity::INFO, PROC_TAG);
    // Logger::getInstance().log(prefix + "    Remote peers:", Logger::Severity::INFO, PROC_TAG);
    // for (auto client : remote_clients)
    // {
    //     client->print(prefix + "    ", period);
    // }
    // Logger::getInstance().log(prefix + "bytes_read: " + std::to_string(received_bytes_per_period), Logger::Severity::INFO, ITF_TAG);
    // Logger::getInstance().log(prefix + "    DS: " + std::to_string(down_speed) + " kbps, US: " + std::to_string(up_speed) + " kbps, RPP: " +
    //         std::to_string(received_bytes_per_period) + ", SPP: " + std::to_string(sent_bytes_per_period), Logger::Severity::INFO, PROC_TAG);
    // Logger::getInstance().log(prefix + "    AV_DS: " + std::to_string(average_down_speed) + " kbps, AV_US: " + std::to_string(average_up_speed) + " kbps", Logger::Severity::INFO, PROC_TAG);
    // Logger::getInstance().log(prefix + "======================\n\n", Logger::Severity::INFO, PROC_TAG);
    // last_print = get_utc();
    // sent_bytes_per_period = 0;
    received_bytes_per_period = 0;
    // received_packets_per_period = 0;
    // sent_packets_per_period = 0;
}

bool MayhemInterface::isRunning()
{
    return !stop_running;
}