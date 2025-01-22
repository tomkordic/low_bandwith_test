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
    cmake \
    iproute2

RUN mkdir -p /install/network_monitor/build
COPY tools/network_monitor/CMakeLists.txt /install/network_monitor/
COPY tools/network_monitor/src /install/network_monitor/src


WORKDIR /install/network_monitor/build
RUN cmake .. && make
RUN chmod +x ./network_monitor

# "ffmpeg" -i "rtsp://admin:koZLd%%40rbsizL5pg3@192.168.1.201:554" -an -vcodec libx264 -vprofile baseline -s 320x240 -b:v 250k -f flv "rtmp://54.160.137.120/show/camera1"
# Command to publish video during the test
CMD ["ffmpeg", "-re", "-stream_loop" , "-1", "-i", "/video_samples/serenety.mp4", "-an" \
        ,"-vcodec", "libx264", "-vprofile", "baseline", "-s", "320x240", "-b:v", "250k", "-f", "flv", "rtmp://nginx-rtmp:1935/show/stream"]