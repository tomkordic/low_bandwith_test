#ifndef NETWORK_MEYHAM_INTERFACE_HPP
#define NETWORK_MEYHAM_INTERFACE_HPP

#include "constants.hpp"
#include "protocol.hpp"

#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <atomic>
#include <pcap.h>
#include <unordered_map>

namespace networkinterface
{
    const int REBROADCAST_INTERVAL = 1000; // ms

    class EthernetPacket
    {
    public:
        EthernetPacket(int index);
        ~EthernetPacket();
        int index;
        EthernetHeader eth_header;
        // struct iphdr *ip_header = nullptr;
        // struct ip6_hdr *ipv6_header = nullptr;
        IPv6Header ipv6_header;
        IPv4Header ipv4_header;
        ARPHeader arp_header;
        struct tcphdr *tcp_header = nullptr;
        struct udphdr *udp_header = nullptr;
        
        bool read_done = false;
        int total_size_no_checksum = 0; // size of packet with all headers withouth 4 bytes checksum

        std::shared_ptr<BufferMixin> parser;
        void print(std::string prefix);
        std::string get_src_ip();
        std::string get_dst_ip();
    };

    class MayhemInterface
    {
    public:
        MayhemInterface(std::string dev, std::string ip, std::string netmask, std::string mac, std::string gateway, std::string bridge);
        ~MayhemInterface();

        bool isRunning();
        void print(std::string prefix);
        std::shared_ptr<EthernetPacket> clone_packet(std::shared_ptr<EthernetPacket> original);
    private:
        void run();
        void process_stdin(char *buffer, int &position, int buffer_size);
        void createInterface();
        void add_default_route(const std::string &gateway);
        void add_ip_address(const std::string &ip_address, const std::string &netmask);
        void set_mac_address();
        void add_to_bridge();
        void get_mac();
        void bring_up_interface();
        void set_non_blocking(int fd);
        void read_packet(std::shared_ptr<EthernetPacket> packet, std::shared_ptr<BufferMixin> parser);
        void write_packet(std::shared_ptr<EthernetPacket> packet);

    private:
        std::string dev;
        std::string bridge;
        std::string mac;
        std::vector<std::shared_ptr<EthernetPacket>> queue;
        std::thread worker;
        std::mutex dataMutex;
        std::atomic<bool> stop_running;
        long received_bytes_per_period;
        long sent_bytes_per_period;
        long received_packets_per_period;
        long sent_packets_per_period;
        long total_sent_bytes;
        long total_received_bytes;
        long last_print;
        long started;
        int fd;
        std::string local_ip;
        std::unordered_map<std::string, long> broadcast_timetable;
    };
} // namespace networkmonitor

#endif // NETWORK_MEYHAM_INTERFACE_HPP