# Use an official base image
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV NGINX_VERSION=1.25.2
ENV RTMP_MODULE_VERSION=1.2.2

# Install required packages for building and running NGINX
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    libpcre3 \
    libpcre3-dev \
    zlib1g \
    zlib1g-dev \
    libssl-dev \
    wget \
    iproute2 \
    iputils-ping \
    net-tools \
    ffmpeg \
    tcpdump \
    libpcap-dev \
    cmake \
    curl && \
    apt-get clean

# Set the CFLAGS environment variable
ENV CFLAGS="$CFLAGS -Wno-error"

# Copy and install NGINX with nginx-rtmp-module, we can not use NGINX_VERSION in the COPY command
COPY server/nginx-1.25.2 /install/nginx-1.25.2
COPY server/nginx-rtmp-module-1.2.2 /install/nginx-rtmp-module-1.2.2
RUN cd /install/nginx-${NGINX_VERSION} && \
    ./configure \
      --prefix=/install/nginx \
      --with-http_ssl_module \
      --with-http_v2_module \
      --add-module=../nginx-rtmp-module-${RTMP_MODULE_VERSION} --with-debug && \
    make && \
    make install

# Add NGINX to PATH
ENV PATH="/install/nginx/sbin:${PATH}"

# Create a directory for NGINX logs
RUN mkdir -p /install/nginx/logs
RUN mkdir -p /install/nginx/mnt/hls

# Expose ports (HTTP and RTMP)
EXPOSE 80 1935

# Copy custom configuration files (optional)
# Uncomment the following line if you want to include custom configs
COPY server/nginx.conf /install/nginx/conf/nginx.conf

RUN mkdir -p /install/net_mhm_itf/build
COPY tools/net_mhm_itf/CMakeLists.txt /install/net_mhm_itf/
COPY tools/net_mhm_itf/src /install/net_mhm_itf/src
WORKDIR /install/net_mhm_itf/build
RUN cmake .. && make
RUN chmod +x ./network_mayhem

# Start NGINX
CMD ["nginx", "-g", "daemon off;"]
