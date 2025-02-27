#!/bin/bash

# Build and start publisher and server
docker-compose up --build -d

# Start network monitors
# sleep 2
# docker exec -d  rtmp-publish /bin/bash -c "ip link set dev eth0 down"
docker exec -d  rtmp-publish /bin/bash -c "ip addr del 192.168.100.3/24 dev eth0 && ip link add name br0 type bridge && ip link set br0 up && ip link set eth0 master br0"
docker exec -d  nginx-rtmp /bin/bash -c "ip addr del 192.168.100.2/24 dev eth0 && ip link add name br0 type bridge && ip link set br0 up && ip link set eth0 master br0"

# docker exec -d  rtmp-publish /bin/bash -c "/install/net_mhm_itf/build/network_mayhem -i '192.168.100.3' > /logs/rtmp-publish-network.log"
# docker exec -d  nginx-rtmp   /bin/bash -c "/install/net_mhm_itf/build/network_mayhem -i '192.168.100.2' > /logs/nginx-rtmp-network.log"

# sleep 1
# docker exec -d  rtmp-publish /bin/bash -c \
#     "ffmpeg -re -stream_loop -1 -i /video_samples/serenety.mp4 -an -vcodec libx264 -vprofile baseline -s 320x240 -b:v 250k -f flv rtmp://192.168.100.2:1935/show/stream"


# Build playback image
# echo "build playback image"
# docker build -f ./playback/Dockerfile  -t playback-image .

# # Start playback image
# echo "start playback image"
# docker run -d \
#   --name hls-playback \
#   --cap-add=NET_ADMIN \
#   --privileged=false \
#   --net low_bandwidth_stream_app_net \
#   --ip 192.168.100.4 \
#   --volume ./logs:/logs \
#   playback-image

# # Start playback network monitor
# echo "Starting network monitor on hls-playback"
# docker exec -d  hls-playback /bin/bash -c "/install/network_monitor/build/network_monitor eth0 > /logs/hls-playback-network.log"


## Set bandwidth limits
# echo "Limiting RTMP publish bandwidth to 500kibit"
# docker run --rm \
#   --name pumba-netlimit-publish \
#   --network low_bandwidth_stream_app_net \
#   -v /var/run/docker.sock:/var/run/docker.sock \
#   gaiaadm/pumba netem \
#   --interface eth0 \
#   --duration 30m \
#   delay \
#   --time 600 \
#   loss \
#   --percent 1 \
#   rtmp-publish &

## Set bandwidth limits
# echo "Limiting server and player bandwidth to 100mbit"
# docker run --rm \
#   --name pumba-netlimit-server \
#   --network low_bandwidth_stream_app_net \
#   -v /var/run/docker.sock:/var/run/docker.sock \
#   gaiaadm/pumba netem \
#   --interface eth0 \
#   --duration 30m \
#   delay \
#   --time 300 \
#   nginx-rtmp &

# Print playback logs
# docker logs -f hls-playback > ./logs/hls-playback.log 2>&1 &
docker logs -f nginx-rtmp > ./logs/nginx-rtmp.log 2>&1 &
docker logs -f rtmp-publish > ./logs/rtmp-publish.log 2>&1 &

# Function to handle the interrupt signal (Ctrl+C or other signal)
interrupt_sleep() {
    echo "Sleep interrupted!"
    cleanup
}

# Function to handle cleanup actions
cleanup() {
    echo "Cleanup ..."
    docker stop -t 0 hls-playback
    docker-compose down
    echo "Cleanup done!"
}

# Set up a trap to catch SIGINT (Ctrl+C) and call interrupt_sleep function
trap interrupt_sleep SIGINT

echo "collect stats for 30 min"
sleep 1800
cleanup

## docker exec -it low_bandwidth_stream-nginx-rtmp /bin/bash