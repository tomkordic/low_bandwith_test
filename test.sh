#!/bin/bash

# Build and start publisher and server
docker-compose up --build -d

# Start network monitors
sleep 2
echo "Starting network monitor on rtmp-publish"
docker exec -d  rtmp-publish /bin/bash -c "/install/network_monitor/build/network_monitor eth0 > /logs/rtmp-publish-network.log"
echo "Starting network monitor on nginx-rtmp"
docker exec -d  nginx-rtmp /bin/bash -c "/install/network_monitor/build/network_monitor eth0 > /logs/nginx-rtmp-network.log"


# Build playback image
echo "build playback image"
docker build -f ./playback/Dockerfile  -t playback-image .

# Start playback image
echo "start playback image"
docker run -d \
  --name hls-playback \
  --cap-add=NET_ADMIN \
  --privileged=false \
  --net low_bandwidth_stream_app_net \
  --ip 192.168.100.4 \
  --volume ./logs:/logs \
  playback-image

# Start playback network monitor
echo "Starting network monitor on hls-playback"
docker exec -d  hls-playback /bin/bash -c "/install/network_monitor/build/network_monitor eth0 > /logs/hls-playback-network.log"


## Set bandwidth limits
echo "Limiting RTMP publish bandwidth to 500kibit"
docker run --rm \
  --name pumba-netlimit-publish \
  --network low_bandwidth_stream_app_net \
  -v /var/run/docker.sock:/var/run/docker.sock \
  gaiaadm/pumba netem \
  --interface eth0 \
  --duration 30m \
  delay \
  --time 600 \
  loss \
  --percent 10 \
  rtmp-publish &

## Set bandwidth limits
echo "Limiting server and player bandwidth to 100mbit"
docker run --rm \
  --name pumba-netlimit-server \
  --network low_bandwidth_stream_app_net \
  -v /var/run/docker.sock:/var/run/docker.sock \
  gaiaadm/pumba netem \
  --interface eth0 \
  --duration 30m \
  delay \
  --time 300 \
  nginx-rtmp &

# Print playback logs
docker logs -f hls-playback > ./logs/hls-playback.log 2>&1 &
docker logs -f nginx-rtmp > ./logs/nginx-rtmp.log 2>&1 &
docker logs -f rtmp-publish > ./logs/rtmp-publish.log 2>&1 &

echo "collect stats for 30 min"
sleep 1800

# Tear down services
echo "Cleanup ..."
docker stop pumba-netlimit-publish pumba-netlimit-server
docker stop hls-playback
docker-compose down
echo "Cleanup done!"

## docker exec -it low_bandwidth_stream-nginx-rtmp /bin/bash