FROM debian:bookworm

LABEL org.opencontainers.image.source="https://github.com/fplll/fplll"

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential ninja-build libgmp-dev libmpfr-dev libqd-dev \
        pkg-config python3-venv ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# The Meson in Debian bookworm predates the project's 1.8 floor; install a
# current Meson into an isolated environment.
RUN python3 -m venv /opt/meson && \
    /opt/meson/bin/pip install --no-cache-dir 'meson>=1.8' && \
    ln -s /opt/meson/bin/meson /usr/local/bin/meson

WORKDIR /src/fplll
COPY . .
RUN meson setup build/docker \
        --buildtype=release \
        -Dqd=enabled \
        -Dprefix=/usr/local \
        --default-library=shared \
        -Dtests=false && \
    meson compile -C build/docker && \
    meson install -C build/docker && \
    ldconfig && \
    rm -rf /src/fplll
WORKDIR /
CMD ["/bin/bash"]
