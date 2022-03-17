FROM debian:buster

# Download Raspberry firmware and unpack /opt/vc folder
ADD https://github.com/raspberrypi/firmware/archive/1.20200902.tar.gz /tmp/rpiplay/firmware.tar.gz

RUN cd /tmp/rpiplay && \
    tar -xzvf firmware.tar.gz && \
    cp -R ./firmware-1.20200902/opt/vc /opt/vc && \
    rm -rf ./firmware-1.20200902

# Add sources
ADD . /tmp/rpiplay/RPiPlay-master

# Install essential dependencies
RUN cd /tmp/rpiplay && \
    apt-get update && \
    apt-get -y install cmake build-essential libavahi-compat-libdnssd-dev libplist-dev libssl-dev avahi-discover libnss-mdns

# Build RPiPlay
RUN cd /tmp/rpiplay/RPiPlay-master && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make

WORKDIR /tmp/rpiplay/RPiPlay-master/build

# MISC settings
COPY ./avahi-daemon.conf /etc/avahi/avahi-daemon.conf

USER root
RUN mkdir -p /var/run/dbus

# Run container
EXPOSE 5353 51826
ENTRYPOINT ["/tmp/rpiplay/RPiPlay-master/build/rpiplay"]
