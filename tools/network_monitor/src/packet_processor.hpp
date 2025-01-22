#ifndef NETWORK_PACKET_MONITOR_HPP
#define NETWORK_PACKET_MONITOR_HPP

#include "constants.hpp"

#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <atomic>
#include <pcap.h>
#include <netinet/ip.h> // For IP header structures
#include <arpa/inet.h>  // For inet_ntoa()

namespace networkmonitor
{
    // class NetworkPacket
    // {
    //     long captured_timestamp;
    //     int size;
    // };

    class NetworkClient
    {
    public:
        NetworkClient(std::string srcIP, std::string dstIP);
        void print(std::string prefix, long period);

        std::string srcIP;
        std::string dstIP;
        std::string name;
        long received_bytes;
        long sent_bytes;
        long last_print;
    };

    class PacketProcessor
    {
    public:
        PacketProcessor(char *dev);
        ~PacketProcessor();

        bool isRunning();
        void print(std::string prefix);
    private:
        void monitor();
        static void handlePacket(u_char *userData, const struct pcap_pkthdr *packetHeader, const u_char *packetData);
        void processPacket(const struct pcap_pkthdr *packetHeader, const u_char *packetData);
        bool isIncomingPacket(std::string &ip);
        std::shared_ptr<NetworkClient> getClientForIp(std::string &srcIP, std::string &dstIP);

    private:
        std::vector<std::shared_ptr<NetworkClient>> remote_clients;
        std::string dev;
        char errbuf[PCAP_ERRBUF_SIZE];
        std::vector<std::string> local_ip_addresses;
        pcap_t *handle;

        std::thread processorWorker;
        std::mutex dataMutex;
        std::atomic<bool> stop_running;
        long received_bytes;
        long sent_bytes;
        long last_print;
    };
} // namespace networkmonitor

#endif // NETWORK_PACKET_MONITOR_HPP