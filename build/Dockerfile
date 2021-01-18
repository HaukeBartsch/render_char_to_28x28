FROM ubuntu:20.04
#
# docker build --no-cache -t render_char_to_28x28 build
#
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && apt-get install -yq build-essential \
    cmake git wget libxml2-dev libxslt1-dev libjpeg-dev expat libzip-dev

RUN apt-get install -yq libgdcm-dev libgdcm3.0 libgdcm-tools libboost-dev \
    libboost-system-dev libboost-filesystem-dev libboost-timer-dev libboost-date-time-dev \
    libfreetype-dev

# build gdcm and the executable
# test with 3.0.5 instead of 2.8.9
RUN cd /root && git clone https://github.com/HaukeBartsch/render_char_to_28x28.git \
    && cd render_char_to_28x28 \
    && cmake -DCMAKE_BUILD_TYPE=Release . && make

