# Basic networking
1. [How libpcap captures packets](#how-libpcap-captures-packets)
2. [Understanding Packet Structure](#understanding-packet-structure)
    - [Ethernet Header Structure](#ethernet-header-structure)
    - [IP Header Structure (IPv4)](#ip-header-structure-ipv4)
    - [TCP Header Structure](#tcp-header-structure)
    - [UDP Header Structure](#udp-header-structure)
3. [Understanding OSI Layers](#understanding-osi-layers)
    - [Layer 1: Physical Layer](#layer-1-physical-layer)
    - [Layer 2: Data Link Layer](#layer-2-data-link-layer)
    - [Layer 3: Network Layer](#layer-3-network-layer)
    - [Layer 4: Transport Layer](#layer-4-transport-layer)
4. [Test with Pumba, packet loss 80%](#test-with-pumba-packet-loss-80)
5. [Test with Pumba, packet loss 1%](#test-with-pumba-packet-loss-1)

---

## How libpcap captures packets.
    [Network Interface] --> [Kernel/Driver] --> [BPF Filter] --> [Kernel Buffer] --> [libpcap API] --> [User Application]

## Understanding Packet Structure
A network packet consists of the following layers:
  1. Ethernet Header (Layer 2), ARPA
  2. IP Header (Layer 3), IP header indicates whether the transport protocol is TCP (6) or UDP (17) (in the protocol field).
  3. Transport Layer Header (TCP/UDP - Layer 4)
  4. Payload/Data (Application Layer - Layer 7)
The source port and destination port are part of the TCP or UDP header, which lies after the IP header.

### Ethernet Header Structure
An Ethernet frame consists of the following components:
| Field               | Size           | Description                                                       |
|---------------------|----------------|-------------------------------------------------------------------|
| **Destination MAC** | 6 bytes        | The MAC address of the recipient (target device).                |
| **Source MAC**      | 6 bytes        | The MAC address of the sender (source device).                   |
| **EtherType**       | 2 bytes        | Indicates the protocol type of the payload (e.g., IPv4, IPv6, ARP). |
| **Payload**         | 46–1500 bytes  | The actual data being carried (e.g., IP packet, ARP request/response). |
| **Frame Check Sequence (FCS)** | 4 bytes | CRC checksum for error detection.                               |


### IP Header Structure (IPv4)

| Field                  | Size        | Description                                                              |
|------------------------|-------------|--------------------------------------------------------------------------|
| **Version**            | 4 bits      | IP version (typically 4 for IPv4).                                       |
| **IHL (Header Length)**| 4 bits      | Length of the IP header in 32-bit words. Minimum value is 5 (20 bytes).  |
| **Type of Service (ToS)**| 1 byte    | Differentiated services field for QoS (e.g., prioritize latency or throughput). |
| **Total Length**       | 2 bytes     | Total size of the packet, including header and data, in bytes.           |
| **Identification**     | 2 bytes     | Unique identifier for the packet, used for reassembling fragmented packets. |
| **Flags**              | 3 bits      | Control flags for fragmentation (e.g., "Don't Fragment").                |
| **Fragment Offset**    | 13 bits     | Indicates the position of a fragment within the original packet.         |
| **Time to Live (TTL)** | 1 byte      | Limits the packet's lifetime (hops) to prevent infinite looping.         |
| **Protocol**           | 1 byte      | Indicates the transport layer protocol (e.g., 6 for TCP, 17 for UDP).    |
| **Header Checksum**    | 2 bytes     | Error-checking for the IP header (not the payload).                      |
| **Source IP Address**  | 4 bytes     | The IP address of the sender.                                            |
| **Destination IP Address** | 4 bytes | The IP address of the recipient.                                         |
| **Options (Optional)** | Variable    | Additional options (e.g., security or timestamp).                        |
| **Padding**            | Variable    | Ensures the header ends at a 32-bit boundary.                            |

### IPv6 Header Structure

The IPv6 header is **40 bytes** in size and has the following fields:

| **Field**            | **Size**      | **Description**                                                                                     |
|----------------------|---------------|-----------------------------------------------------------------------------------------------------|
| **Version**          | 4 bits        | IP version, always `6` for IPv6.                                                                    |
| **Traffic Class**    | 8 bits        | Used for packet priority or Quality of Service (QoS).                                               |
| **Flow Label**       | 20 bits       | Identifies traffic flows for special handling (e.g., QoS).                                           |
| **Payload Length**   | 16 bits       | Length of the payload in bytes (excluding the IPv6 header).                                          |
| **Next Header**      | 8 bits        | Specifies the type of the next header or protocol (e.g., TCP, UDP, or extension headers).            |
| **Hop Limit**        | 8 bits        | The maximum number of hops (similar to IPv4 TTL); decremented at each hop.                          |
| **Source Address**   | 128 bits (16 bytes) | The IPv6 address of the sender.                                                                 |
| **Destination Address** | 128 bits (16 bytes) | The IPv6 address of the recipient.                                                          |


### TCP Header Structure

| Field                  | Size        | Description                                                             |
|------------------------|-------------|-------------------------------------------------------------------------|
| **Source Port**        | 2 bytes     | The port number of the sending application.                             |
| **Destination Port**   | 2 bytes     | The port number of the receiving application.                           |
| **Sequence Number**    | 4 bytes     | Indicates the sequence number of the first byte in the packet.          |
| **Acknowledgment Number** | 4 bytes  | If the ACK flag is set, this field contains the next sequence number expected. |
| **Data Offset**        | 4 bits      | The size of the TCP header in 32-bit words.                             |
| **Reserved**           | 3 bits      | Reserved for future use, should be set to 0.                            |
| **Flags**              | 9 bits      | Control flags, e.g., SYN, ACK, FIN, etc.                                |
| **Window Size**        | 2 bytes     | The size of the receive window (buffer space available).                |
| **Checksum**           | 2 bytes     | Error-checking for the TCP header and data.                             |
| **Urgent Pointer**     | 2 bytes     | If the URG flag is set, this points to urgent data.                     |
| **Options**            | Variable    | Optional settings such as Maximum Segment Size (MSS).                   |
| **Padding**            | Variable    | Ensures the header ends at a 32-bit boundary.                           |

### UDP Header Structure

| Field              | Size     | Description                                                   |
|--------------------|----------|---------------------------------------------------------------|
| **Source Port**    | 2 bytes  | The port number of the sending application.                   |
| **Destination Port** | 2 bytes | The port number of the receiving application.                |
| **Length**         | 2 bytes  | The total length of the UDP header and data, in bytes.        |
| **Checksum**       | 2 bytes  | Error-checking for the UDP header and data.                   |

## Understanding OSI layers

**Layer 1: Physical Layer**
   - Protocol/Technology Examples:
     - Ethernet (physical aspects, like cabling and signaling)
     - DSL (Digital Subscriber Line)
     - Optical Fiber
     - Wi-Fi (physical radio transmission)
     - Bluetooth


**Layer 2: Data Link Layer**
   - Protocol Examples:
     - Ethernet
     - PPP (Point-to-Point Protocol)
     - Wi-Fi (IEEE 802.11)
     - MPLS (Multiprotocol Label Switching)
     - Frame Relay

**Layer 3: Network Layer**
   - Protocol Examples:
     - IPv4 (Internet Protocol version 4)
     - IPv6 (Internet Protocol version 6)
     - ICMP (Internet Control Message Protocol)
     - ARP (Address Resolution Protocol)
     - RIP (Routing Information Protocol)

**Layer 4: Transport Layer**
   - Protocol Examples:
     - TCP (Transmission Control Protocol)
     - UDP (User Datagram Protocol)
     - SCTP (Stream Control Transmission Protocol)



#### Test with Pumba, packet loss 80%
    2025-01-22 23:01:21.562 [INFO] [NET_PROCESSOR] =====  NET STATS =====
    2025-01-22 23:01:21.562 [INFO] [NET_PROCESSOR]     Remote peers:
    2025-01-22 23:01:21.562 [INFO] [NET_PROCESSOR]     === 103d43c267d4<192.168.100.3> ===
    2025-01-22 23:01:21.562 [INFO] [NET_PROCESSOR]         DS: 1077 kbps, US: 252 kbps
    2025-01-22 23:01:21.563 [INFO] [NET_PROCESSOR]         AV_DS: 739 kbps, AV_US: 198 kbps
    2025-01-22 23:01:21.563 [INFO] [NET_PROCESSOR] ======== TOTAL =======
    2025-01-22 23:01:21.563 [INFO] [NET_PROCESSOR]     DS: 252 kbps, US: 1077 kbps, RPP: 252567, SPP: 1080043
    2025-01-22 23:01:21.563 [INFO] [NET_PROCESSOR]     AV_DS: 196 kbps, AV_US: 732 kbps
    2025-01-22 23:01:21.563 [INFO] [NET_PROCESSOR] ======================

#### Test with Pumba, packet loss 1%
    2025-01-22 23:37:22.959 [INFO] [NET_PROCESSOR] =====  NET STATS =====
    2025-01-22 23:37:22.959 [INFO] [NET_PROCESSOR]     Remote peers:
    2025-01-22 23:37:22.959 [INFO] [NET_PROCESSOR]     === 9bb5235e9d3e<192.168.100.3> ===
    2025-01-22 23:37:22.960 [INFO] [NET_PROCESSOR]         DS: 1167 kbps, US: 279 kbps
    2025-01-22 23:37:22.960 [INFO] [NET_PROCESSOR]         AV_DS: 690 kbps, AV_US: 184 kbps
    2025-01-22 23:37:22.960 [INFO] [NET_PROCESSOR] ======== TOTAL =======
    2025-01-22 23:37:22.960 [INFO] [NET_PROCESSOR]     DS: 279 kbps, US: 1167 kbps, RPP: 279153, SPP: 1167833
    2025-01-22 23:37:22.960 [INFO] [NET_PROCESSOR]     AV_DS: 183 kbps, AV_US: 685 kbps
    2025-01-22 23:37:22.960 [INFO] [NET_PROCESSOR] ======================

