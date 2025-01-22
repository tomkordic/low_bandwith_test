FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

## cmake, pkg-config, maybe ffmpeg-dev

RUN apt-get update && \
    apt-get install -y \
    python3 \
    python3-pip \
    build-essential \
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libcurl4-openssl-dev \
    cmake \
    pkg-config \
    libpcap-dev \
    iproute2 && \
    apt-get clean

# Install dependencies
RUN pip install requests

# build network monitor
RUN mkdir -p /install/network_monitor/build
COPY tools/network_monitor/CMakeLists.txt /install/network_monitor/
COPY tools/network_monitor/src /install/network_monitor/src


WORKDIR /install/network_monitor/build
RUN cmake .. && make
RUN chmod +x ./network_monitor

# Build client
RUN mkdir -p /install/playback_test/build
WORKDIR /install/playback_test/build

COPY playback/playback_test/CMakeLists.txt /install/playback_test/
COPY playback/playback_test/src /install/playback_test/src

RUN cmake .. && make


# Run the test script
CMD ["./PlaybackVerifier", "http://nginx-rtmp/hls/stream.m3u8"]