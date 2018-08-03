FROM ubuntu:16.04

RUN apt-get update -y
RUN apt-get install -y \
    vim \
    curl \
    git-core \
    pkg-config \
    glibmm-2.4 \
    libssl-dev \
    gdb \
    lsof \
    libcurl4-openssl-dev \
    software-properties-common \
    build-essential \
    python-dev \
    autotools-dev \
    libicu-dev \
    build-essential \
    libbz2-dev

# Install clang
RUN curl -L https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN apt-add-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-6.0 main"
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y
RUN apt-get update
RUN apt-get install -y \
    clang-6.0 \
    gcc-6 \
    g++-6
RUN ln -s /usr/bin/g++-6.0 /usr/bin/g++
RUN ln -s /usr/bin/gcc-6.0 /usr/bin/gcc
RUN ln -s /usr/bin/clang-6.0 /usr/bin/clang++
RUN ln -s /usr/bin/clang-6.0 /usr/bin/clang

RUN curl -L https://cmake.org/files/v3.12/cmake-3.12.0.tar.gz -o cmake-3.12.0.tar.gz
RUN tar xvfz cmake-3.12.0.tar.gz

WORKDIR cmake-3.12.0
RUN ./bootstrap
RUN make -j$(nproc)
RUN make install

WORKDIR ../

# Install Boost
RUN curl -L https://dl.bintray.com/boostorg/release/1.67.0/source/boost_1_67_0.tar.gz -o boost_1_67_0.tar.gz
RUN echo "8aa4e330c870ef50a896634c931adf468b21f8a69b77007e45c444151229f665 boost_1_67_0.tar.gz" | sha256sum -c
RUN tar xzvf boost_1_67_0.tar.gz
WORKDIR boost_1_67_0
RUN ./bootstrap.sh
RUN ./b2 toolset=clang -j$(nproc)
RUN ./b2 install

WORKDIR ../

RUN mkdir flashpoint

WORKDIR flashpoint

CMD exec /bin/bash -c "tail -f /dev/null"