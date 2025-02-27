#ifndef NETWORK_MEYHAM_PROTOCOL_HPP
#define NETWORK_MEYHAM_PROTOCOL_HPP

#include "logger.hpp"
#include "binary_parser.hpp"
#include "constants.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace networkinterface
{

    static const int ETHERNET_PACKET_MAX_SIZE = 1518;
    static const int ETHERNET_PACKET_HEADER_SIZE = 14;
    static const int INTERNET_PACKET_HEADER_MINIMAL_LENGTH = 20;
    static const int ARP_PACKET_HEADER_MINIMAL_LENGTH = 28;
    static const int IPV6_HEADER_LENGTH = 40;
    static const int FRAME_CHECK_SEQUENCE_LENGTH = 4;
    static const int MAX_ARP_PROTOCOL_ADDR_LEN = 16;

    enum class EthernetType
    {
        IPv4 = 0x0800,                  // IPv4 Internet Protocol version 4
        IPv6 = 0x86DD,                  // IPv6 Internet Protocol version 6
        ARP = 0x0806,                   // Address Resolution Protocol
        Wake_on_LAN = 0x0842,           // Wake-on-LAN packets
        IEEE_802_1Q = 0x8100,           // VLAN-tagged frame (IEEE 802.1Q)
        LLDP = 0x88CC,                  // Link Layer Discovery Protocol
        Ethernet_Flow_Control = 0x8808, // Ethernet Flow Control (IEEE 802.3 pause frame)
        X_25 = 0x0805,                  // CCITT X.25 PLP
        IPX = 0x8137,                   // Novell Internet Packet Exchange
        MACSec = 0x88E5,                // MACSec (IEEE 802.1AE Security Protocol)
        MPLS_Unicast = 0x8847,          // MPLS Unicast (Multiprotocol Label Switching)
        MPLS_Multicast = 0x8848,        // MPLS Multicast
        PPPoE_Discovery = 0x8863,       // Point-to-Point Protocol over Ethernet (Discovery Stage)
        PPPoE_Session = 0x8864,         // Point-to-Point Protocol over Ethernet (Session Stage)
        PTP = 0x88F7,                   // Precision Time Protocol (IEEE 1588)
        Unknown = 0x0000                // Unknown or unsupported EtherType
    };

    enum class IpProtocol
    {
        ICMP = 1,       // Internet Control Message Protocol
        TCP = 6,        // Transmission Control Protocol
        UDP = 17,       // User Datagram Protocol
        IPv6 = 41,      // IPv6 encapsulated in IPv4
        GRE_IPv4 = 47,  // Generic Routing Encapsulation (IPv4)
        ESP = 50,       // Encapsulating Security Payload (IPsec)
        AH = 51,        // Authentication Header (IPsec)
        GRE_IPv6 = 128, // Generic Routing Encapsulation (IPv6)
        Unknown = 0     // Unknown or unsupported protocol
    };

    enum class ArpOperations {
        ARP_REQUEST    = 1,  // Who has this IP? Tell me. Used to ask for the MAC address corresponding to an IP address.
        ARP_REPLY      = 2,  // Here is the MAC for this IP. Response providing the requested MAC address.
        RARP_REQUEST   = 3,  // Who am I? Tell me my IP. (Reverse ARP) Requests IP address for a given MAC address. (Rarely used now)
        RARP_REPLY     = 4,  // Your IP address is ... (Reverse ARP reply). (Deprecated in modern networks)
        IN_ARP_REQUEST = 8,  // What is your IP? Used in Inverse ARP to discover peer IP addresses in Frame Relay and ATM networks.
        IN_ARP_REPLY   = 9,  // My IP address is ... Reply for InARP request.
        ARP_NAK        = 10  // ARP Negative Acknowledgement. Used in some proprietary implementations to indicate an IP cannot be resolved. (Not standardized in RFC 826)
    };

    class EthernetHeader
    {
    public:
        uint8_t *dest_mac;      // Destination mac address, 6 bytes
        uint8_t *src_mac;       // Source mac address, 6 bytes
        EthernetType ethertype; // 2 bytes
        void parse(std::shared_ptr<BufferMixin> parser)
        {
            Logger::getInstance().log("Eth (hex): " + parser->hex(parser->_position, ETHERNET_PACKET_HEADER_SIZE), Logger::Severity::DEBUG, "ITF_TAG");
            dest_mac = parser->rbytes(6);
            src_mac = parser->rbytes(6);
            ethertype = (EthernetType)parser->ru16();
            Logger::getInstance().log("New Packet, type: " + ethernet_type_to_str(), Logger::Severity::DEBUG, "ITF_TAG");
        }

        std::string ethernet_type_to_str()
        {
            switch (ethertype)
            {
            case EthernetType::IPv4:
                return "IPv4";
            case EthernetType::IPv6:
                return "IPv6";
            case EthernetType::ARP:
                return "ARP";
            case EthernetType::Wake_on_LAN:
                return "Wake-on-LAN";
            case EthernetType::IEEE_802_1Q:
                return "VLAN-tagged frame (IEEE 802.1Q)";
            case EthernetType::LLDP:
                return "Link Layer Discovery Protocol (LLDP)";
            case EthernetType::Ethernet_Flow_Control:
                return "Ethernet Flow Control";
            case EthernetType::X_25:
                return "CCITT X.25";
            case EthernetType::IPX:
                return "Novell Internet Packet Exchange (IPX)";
            case EthernetType::MACSec:
                return "MACSec (IEEE 802.1AE)";
            case EthernetType::MPLS_Unicast:
                return "MPLS Unicast";
            case EthernetType::MPLS_Multicast:
                return "MPLS Multicast";
            case EthernetType::PPPoE_Discovery:
                return "PPPoE Discovery";
            case EthernetType::PPPoE_Session:
                return "PPPoE Session";
            case EthernetType::PTP:
                return "Precision Time Protocol (PTP)";
            case EthernetType::Unknown:
                return "Unknown EtherType";
            }
            return "Unknown: " + std::to_string((int)ethertype);
        }
    };

    enum class ARPOperationType {
        REQUEST = 1,
        REPLY
    };

    // ARP Header Class
    class ARPHeader
    {
    public:
        uint16_t hardware_type;    // Hardware Type
        uint16_t protocol_type;    // Protocol Type
        uint8_t hardware_addr_len; // Hardware Address Length
        uint8_t protocol_addr_len; // Protocol Address Length
        uint16_t operation;        // Operation (Request/Reply)
        uint8_t *sender_mac;       // Sender MAC Address
        uint8_t *sender_ip;        // Sender IP Address
        uint8_t *target_mac;       // Target MAC Address
        uint8_t *target_ip;        // Target IP Address

        void set_dest_mac(uint8_t *mac, std::shared_ptr<BufferMixin> parser) {
            auto pos =  parser->_position;
            parser->seek(0);
            // Mac address length always 6 bytes for ethernet packet header
            parser->wbytes(mac, MAC_ADDR_LEN);
            parser->seek(22);
            // This could be longer but for now we will assume it is 6
            parser->wbytes(mac, MAC_ADDR_LEN);
            parser->seek(pos);
        }

        void set_src_mac(uint8_t *mac, std::shared_ptr<BufferMixin> parser) {
            auto pos =  parser->_position;
            parser->seek(MAC_ADDR_LEN);
            // Mac address length always 6 bytes for ethernet packet header
            parser->wbytes(mac, MAC_ADDR_LEN);
            parser->seek(22 + MAC_ADDR_LEN + protocol_addr_len);
            // This could be longer but for now we will assume it is 6
            parser->wbytes(mac, MAC_ADDR_LEN);
            parser->seek(pos);
        }

        void set_dest_ip(uint8_t *ip, std::shared_ptr<BufferMixin> parser) {
            auto pos =  parser->_position;
            parser->seek(22 + (MAC_ADDR_LEN * 2) + protocol_addr_len);
            parser->wbytes(ip, protocol_addr_len);
            parser->seek(pos);
        }

        void set_src_ip(uint8_t *ip, std::shared_ptr<BufferMixin> parser) {
            auto pos =  parser->_position;
            parser->seek(22 + MAC_ADDR_LEN);
            parser->wbytes(ip, protocol_addr_len);
            parser->seek(pos);
        }

        void set_operation(ArpOperations value, std::shared_ptr<BufferMixin> parser) {
            operation = (uint16_t)value;
            int64_t pos = parser->_position;
            parser->seek(20);
            parser->wu16((uint16_t)value);
            parser->seek(pos);
        }

        void parse(std::shared_ptr<BufferMixin> parser)
        {
            Logger::getInstance().log("ARP (hex): " + parser->hex(parser->_position, parser->_bytes_written), Logger::Severity::DEBUG, "ITF_TAG");
            hardware_type = parser->ru16();
            protocol_type = parser->ru16();
            hardware_addr_len = parser->ru8();
            protocol_addr_len = parser->ru8();
            operation = parser->ru16();
            sender_mac = parser->rbytes(hardware_addr_len);
            sender_ip = parser->rbytes(protocol_addr_len);
            target_mac = parser->rbytes(hardware_addr_len);
            target_ip = parser->rbytes(protocol_addr_len);
        }
    };

    // IPv4 Header Class
    class IPv4Header
    {
    public:
        uint8_t version_ihl;            // Version (4 bits) + Internet Header Length (4 bits)
        uint8_t type_of_service;        // Type of Service
        uint16_t total_length;          // Total Length
        uint16_t identification;        // Identification
        uint16_t flags_fragment_offset; // Flags (3 bits) + Fragment Offset (13 bits)
        uint8_t ttl;                    // Time to Live
        uint8_t protocol;               // Protocol
        uint16_t header_checksum;       // Header Checksum
        uint32_t source_ip;             // Source Address
        uint32_t dest_ip;               // Destination Address

        void parse(uint8_t *buffer)
        {
            version_ihl = buffer[0];
            type_of_service = buffer[1];
            total_length = (buffer[2] << 8) | buffer[3];
            identification = (buffer[4] << 8) | buffer[5];
            flags_fragment_offset = (buffer[6] << 8) | buffer[7];
            ttl = buffer[8];
            protocol = buffer[9];
            header_checksum = (buffer[10] << 8) | buffer[11];
            source_ip = (buffer[12] << 24) | (buffer[13] << 16) | (buffer[14] << 8) | buffer[15];
            dest_ip = (buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19];
        }

        void serialize(uint8_t *buffer, size_t length) const
        {
            if (length < 20)
            {
                throw std::runtime_error("Invalid buffer size: too small for IPv4 header.");
            }
            buffer[0] = version_ihl;
            buffer[1] = type_of_service;
            buffer[2] = total_length >> 8;
            buffer[3] = total_length & 0xFF;
            buffer[4] = identification >> 8;
            buffer[5] = identification & 0xFF;
            buffer[6] = flags_fragment_offset >> 8;
            buffer[7] = flags_fragment_offset & 0xFF;
            buffer[8] = ttl;
            buffer[9] = protocol;
            buffer[10] = header_checksum >> 8;
            buffer[11] = header_checksum & 0xFF;
            buffer[12] = source_ip >> 24;
            buffer[13] = (source_ip >> 16) & 0xFF;
            buffer[14] = (source_ip >> 8) & 0xFF;
            buffer[15] = source_ip & 0xFF;
            buffer[16] = dest_ip >> 24;
            buffer[17] = (dest_ip >> 16) & 0xFF;
            buffer[18] = (dest_ip >> 8) & 0xFF;
            buffer[19] = dest_ip & 0xFF;
        }

        uint8_t get_version() const
        {
            return version_ihl >> 4;
        }

        uint8_t get_header_length() const
        {
            return (version_ihl & 0x0F) * 4;
        }
    };

    // IPv6 Header Class
    class IPv6Header
    {
    public:
        uint32_t version_tc_flowlabel; // Version (4 bits) + Traffic Class (8 bits) + Flow Label (20 bits)
        uint16_t payload_length;       // Payload Length
        uint8_t next_header;           // Next Header
        uint8_t hop_limit;             // Hop Limit
        uint8_t *source_ip;            // Source Address
        uint8_t *dest_ip;              // Destination Address

        void parse(std::shared_ptr<BufferMixin> parser)
        {
            Logger::getInstance().log("IPv6 (hex): " + parser->hex(0, IPV6_HEADER_LENGTH + ETHERNET_PACKET_HEADER_SIZE), Logger::Severity::DEBUG, "ITF_TAG");
            version_tc_flowlabel = parser->ru32();
            payload_length = parser->ru16();
            Logger::getInstance().log("IPv6 PL: " + std::to_string(payload_length) + ", prp: " + std::to_string(parser->_position), Logger::Severity::DEBUG, "ITF_TAG");
            next_header = parser->ru8();
            hop_limit = parser->ru8();
            source_ip = parser->rbytes(16);
            dest_ip = parser->rbytes(16);
        }

        void serialize(uint8_t *buffer, size_t length) const
        {
            if (length < 40)
            {
                throw std::runtime_error("Invalid buffer size: too small for IPv6 header.");
            }
            buffer[0] = version_tc_flowlabel >> 24;
            buffer[1] = (version_tc_flowlabel >> 16) & 0xFF;
            buffer[2] = (version_tc_flowlabel >> 8) & 0xFF;
            buffer[3] = version_tc_flowlabel & 0xFF;
            buffer[4] = payload_length >> 8;
            buffer[5] = payload_length & 0xFF;
            buffer[6] = next_header;
            buffer[7] = hop_limit;
            std::memcpy(buffer + 8, source_ip, 16);
            std::memcpy(buffer + 24, dest_ip, 16);
        }

        uint8_t get_version() const
        {
            return (version_tc_flowlabel >> 28) & 0x0F;
        }
    };

    
}
#endif // NETWORK_MEYHAM_PROTOCOL_HPP
