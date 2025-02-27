# Use an official base image
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

RUN mkdir -p /video_samples

# Install required packages
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    libpcap-dev \
    ffmpeg \
    bash \
    nload \
    iputils-ping \
    tcpdump \
    net-tools \
    graphviz \
    cmake \
    iproute2

RUN mkdir -p /install/net_mhm_itf/build
COPY tools/net_mhm_itf/CMakeLists.txt /install/net_mhm_itf/
COPY tools/net_mhm_itf/src /install/net_mhm_itf/src
# COPY tools/net_mhm_itf/libs /install/net_mhm_itf/libs
# RUN mkdir -p /install/net_mhm_itf/libs/libviface/build
# WORKDIR /install/net_mhm_itf/libs/libviface/build
# RUN cmake .. && make && make install

WORKDIR /install/net_mhm_itf/build
RUN cmake .. && make
RUN chmod +x ./network_mayhem
# RUN ./network_mayhem > /logs/rtmp-publish-network.log &
# RUN ip link set dev eth0 down
# RUN ip link set dev mhm0 up
# RUN ip addr add 192.168.100.1/24 dev mhm0
# RUN ip route add default via 192.168.100.1 dev mhm0

# "ffmpeg" -i "rtsp://admin:koZLd%%40rbsizL5pg3@192.168.1.201:554" -an -vcodec libx264 -vprofile baseline -s 320x240 -b:v 250k -f flv "rtmp://54.160.137.120/show/camera1"
# Command to publish video during the test
# CMD ["ffmpeg", "-re", "-stream_loop" , "-1", "-i", "/video_samples/serenety.mp4", "-an" \
#         ,"-vcodec", "libx264", "-vprofile", "baseline", "-s", "320x240", "-b:v", "250k", "-f", "flv", "rtmp://192.168.100.2:1935/show/stream"]
CMD ["tail", "-f", "/dev/null"]