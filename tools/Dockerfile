FROM debian:bookworm

LABEL org.opencontainers.image.source="https://github.com/fplll/fplll"

ARG JOBS=2
ARG CXXFLAGS="-O2 -march=x86-64"
ARG FPLLL_VERSION=""

RUN apt-get update && \
    apt-get install -y --no-install-recommends build-essential cmake ninja-build \
        libgmp-dev libmpfr-dev libqd-dev pkg-config && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src/fplll
COPY . .
RUN cmake -S . -B build/docker -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
        -DFPLLL_VERSION="$FPLLL_VERSION" \
        -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_TESTING=OFF -DFPLLL_WITH_QD=ON && \
    cmake --build build/docker --parallel "$JOBS" && \
    cmake --install build/docker && \
    ldconfig && \
    rm -rf /src/fplll
WORKDIR /
CMD ["/bin/bash"]
