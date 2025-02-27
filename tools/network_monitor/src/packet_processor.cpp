#include "packet_processor.hpp"
#include "logger.hpp"

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>

using namespace networkmonitor;

constexpr const char *PROC_TAG = "NET_PROCESSOR";

NetworkClient::NetworkClient(std::string srcIP, std::string dstIP) : srcIP(srcIP), dstIP(dstIP), 
    total_received_bytes(0), total_sent_bytes(0), received_bytes_per_period(0), sent_bytes_per_period(0), 
    last_print(get_utc()), started(get_utc())
{
    name = ip_to_hostname(srcIP);
    if (name.compare(srcIP) == 0)
    {
        name = "Client";
    }
    Logger::getInstance().log("Domain name for IP " + srcIP + ": " + name, Logger::Severity::INFO, PROC_TAG);
}

void NetworkClient::print(std::string prefix, long period)
{
    if (sent_bytes_per_period == 0 && received_bytes_per_period == 0)
    {
        return;
    }
    long up_speed = sent_bytes_per_period  / (period);
    long down_speed = received_bytes_per_period  / (period);
    long average_up_speed = total_sent_bytes / ((get_utc() - started));
    long average_down_speed = total_received_bytes / ((get_utc() - started));
    Logger::getInstance().log(prefix + "=== " + name + "<" + srcIP + "> ===", Logger::Severity::INFO, PROC_TAG);
    Logger::getInstance().log(prefix + "    DS: " + std::to_string(down_speed) + " kbps, US: " + std::to_string(up_speed) + " kbps", Logger::Severity::INFO, PROC_TAG);
    Logger::getInstance().log(prefix + "    AV_DS: " + std::to_string(average_down_speed) + " kbps, AV_US: " + std::to_string(average_up_speed) + " kbps", Logger::Severity::INFO, PROC_TAG);
    // Logger::getInstance().log(prefix + "-----------------------------" ,Logger::Severity::INFO, PROC_TAG);
    last_print = get_utc();
    sent_bytes_per_period = 0;
    received_bytes_per_period = 0;
}

PacketProcessor::PacketProcessor(char *dev) : dev(dev), stop_running(false), 
    total_received_bytes(0), total_sent_bytes(0), received_bytes_per_period(0), sent_bytes_per_period(0), 
    received_packets_per_period(0), sent_packets_per_period(0),
    last_print(get_utc()), started(get_utc())
{
    Logger::getInstance().log("\n\n ======= Network monitor started ... ======\n\n", Logger::Severity::INFO, PROC_TAG);
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];
    if (getifaddrs(&ifaddr) == -1)
    {
        Logger::getInstance().log("getifaddrs failed !!!", Logger::Severity::ERROR, PROC_TAG);
        throw std::runtime_error("Failed to obtain local ip address");
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
            Logger::getInstance().log("Interface " + std::string(ifa->ifa_name) + " has IP address: " + std::string(ip), Logger::Severity::INFO, PROC_TAG);
            local_ip_addresses.push_back(std::string(ip));
        }
    }
    freeifaddrs(ifaddr);
    processorWorker = std::thread(&PacketProcessor::monitor, this);
}

PacketProcessor::~PacketProcessor()
{
    // Stop running and wait for the thread to finish
    stop_running = true;
    if (processorWorker.joinable())
    {
        processorWorker.join();
    }
}

bool PacketProcessor::isIncomingPacket(std::string &ip)
{
    for (std::string local_ip : local_ip_addresses)
    {
        if (local_ip.compare(ip) == 0)
        {
            return true;
        }
    }
    return false;
}

void PacketProcessor::handlePacket(u_char *userData, const struct pcap_pkthdr *packetHeader, const u_char *packetData)
{
    PacketProcessor *processor = reinterpret_cast<PacketProcessor *>(userData);
    processor->processPacket(packetHeader, packetData);
}

std::shared_ptr<NetworkClient> PacketProcessor::getClientForIp(std::string &srcIP, std::string &dstIP)
{
    for (auto client : remote_clients)
    {
        if (client->srcIP.compare(srcIP) == 0)
        {
            return client;
        }
    }
    std::shared_ptr<NetworkClient> client = std::make_shared<NetworkClient>(srcIP, dstIP);
    remote_clients.push_back(client);
    return client;
}

void PacketProcessor::processPacket(const struct pcap_pkthdr *packetHeader, const u_char *packetData)
{
    const int ethernetHeaderSize = 14; // For Ethernet
    if (packetHeader->caplen < ethernetHeaderSize + sizeof(struct ip)) {
        // Packet too small to contain IP header
        return;
    }

    // Check if the packet is IPv4
    if (ntohs(*(uint16_t *)(packetData + 12)) != 0x0800) {
        return; // Ignore non-IPv4 packets
    }

    // Extract the IP header
    struct ip *ipHeader = (struct ip *)(packetData + ethernetHeaderSize);
    int ipHeaderLength = ipHeader->ip_hl * 4; // IP header length in bytes
    int payloadLength = ipHeader->ip_len - ipHeaderLength;

    std::string srcIP = inet_ntoa(ipHeader->ip_src);
    std::string dstIP = inet_ntoa(ipHeader->ip_dst);

    std::lock_guard<std::mutex> lock(dataMutex);
    if (isIncomingPacket(dstIP))
    {
        std::shared_ptr<NetworkClient> remote = getClientForIp(dstIP, srcIP);
        received_bytes_per_period++;
        received_bytes_per_period += payloadLength;
        total_received_bytes += payloadLength;
        remote->sent_bytes_per_period += payloadLength;
        remote->total_sent_bytes += payloadLength;
    }
    else
    {
        std::shared_ptr<NetworkClient> remote = getClientForIp(srcIP, dstIP);
        sent_bytes_per_period++;
        sent_bytes_per_period += payloadLength;
        total_sent_bytes += payloadLength;
        remote->received_bytes_per_period += payloadLength;
        remote->total_received_bytes += payloadLength;
    }
}

void PacketProcessor::monitor()
{
    // Open the device for packet capture
    handle = pcap_open_live(dev.c_str(), BUFSIZ, 1, 1000, errbuf);
    if (handle == nullptr)
    {
        std::ostringstream msg;
        msg << "Error opening device " << dev << ": " << errbuf;
        Logger::getInstance().log(msg, Logger::Severity::ERROR, PROC_TAG);
        throw std::runtime_error(msg.str());
        return;
    }

    Logger::getInstance().log("Capturing packets on interface: " + dev, Logger::Severity::INFO, PROC_TAG);

    int num_of_error_packets = 0;
    if (pcap_loop(handle, 0, handlePacket, reinterpret_cast<u_char *>(this)) < 0)
    {
        Logger::getInstance().log("Error capturing packets: " + std::string(pcap_geterr(handle)), Logger::Severity::ERROR, PROC_TAG);
    }

    // Close the handle when done
    pcap_close(handle);
    return;
}

void PacketProcessor::print(std::string prefix)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    long period = get_utc() - last_print;
    long up_speed = sent_bytes_per_period  / (period);       // kbps
    long down_speed = received_bytes_per_period  / (period); // kbps
    long average_up_speed = total_sent_bytes / ((get_utc() - started));
    long average_down_speed = total_received_bytes / ((get_utc() - started));
    Logger::getInstance().log(prefix + "=====  NET STATS =====", Logger::Severity::INFO, PROC_TAG);
    Logger::getInstance().log(prefix + "    Remote peers:", Logger::Severity::INFO, PROC_TAG);
    for (auto client : remote_clients)
    {
        client->print(prefix + "    ", period);
    }
    Logger::getInstance().log(prefix + "======== TOTAL =======", Logger::Severity::INFO, PROC_TAG);
    Logger::getInstance().log(prefix + "    DS: " + std::to_string(down_speed) + " kbps, US: " + std::to_string(up_speed) + " kbps, RPP: " +
            std::to_string(received_bytes_per_period) + ", SPP: " + std::to_string(sent_bytes_per_period), Logger::Severity::INFO, PROC_TAG);
    Logger::getInstance().log(prefix + "    AV_DS: " + std::to_string(average_down_speed) + " kbps, AV_US: " + std::to_string(average_up_speed) + " kbps", Logger::Severity::INFO, PROC_TAG);
    Logger::getInstance().log(prefix + "======================\n\n", Logger::Severity::INFO, PROC_TAG);
    last_print = get_utc();
    sent_bytes_per_period = 0;
    received_bytes_per_period = 0;
    received_packets_per_period = 0;
    sent_packets_per_period = 0;
}

bool PacketProcessor::isRunning()
{
    return !stop_running;
}