FROM ubuntu:latest


ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install apt-utils
RUN apt-get -y install curl python3 python3-serial python3-venv python3-pip
#RUN apt-get -y install python3-protobuf python3-grpc-tools

RUN curl -fsSL \
    https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py \
    -o get-platformio.py
RUN python3 get-platformio.py

ENV PATH=/root/.platformio/penv/bin:$PATH
RUN pio pkg install --global --platform espressif32
RUN pio pkg install --global --tool framework-arduinoespressif32
RUN pio pkg install --global --tool tool-scons
RUN pio pkg install --global --library "nanopb/Nanopb"

RUN mkdir -p "/root/project"
VOLUME ["/root/project"]

RUN mkdir -p "/output"
VOLUME ["/output"]

RUN mkdir -p "/root/work"
VOLUME ["/root/work"]

COPY entrypoint.sh /
COPY create_manifest.py /root/
ENTRYPOINT /entrypoint.sh

WORKDIR /root/build
