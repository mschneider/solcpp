FROM ubuntu:18.04

RUN apt update
RUN apt install -y \
  software-properties-common \
  lsb-release \
  wget

RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt update

RUN apt install -y \
  build-essential \
  cmake \
  doctest-dev \
  git \
  libboost-all-dev \
  libcurl4-openssl-dev \
  libsodium-dev \
  libssl-dev \
  libwebsocketpp-dev \
  pkg-config

COPY . /

RUN mkdir -p build
WORKDIR /build
RUN cmake ..
RUN make