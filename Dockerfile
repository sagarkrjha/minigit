# ==============================================================================
# Multi-stage Dockerfile for MiniGit and MiniGit SDK
# Builds portable binaries, headers, and runtime environment
# ==============================================================================

# Stage 1: Build environment
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    libssl-dev \
    zlib1g-dev \
    libcurl4-openssl-dev \
    ca-certificates \
    pkg-config \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Copy build manifest and project source
COPY . .

# Build MiniGit and MiniGit SDK
RUN cmake -B build -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON

RUN cmake --build build --parallel

# Run test suite inside container build
RUN ctest --test-dir build --output-on-failure

# Stage 2: Runtime & Distribution image
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    zlib1g \
    libcurl4 \
    ca-certificates \
    bash \
    && rm -rf /var/lib/apt/lists/*

# Install CLI binary
COPY --from=builder /workspace/build/minigit /usr/local/bin/minigit

# Install SDK headers & static library
COPY --from=builder /workspace/src/sdk/minigit_sdk.h /usr/local/include/minigit/minigit_sdk.h
COPY --from=builder /workspace/build/src/sdk/libminigit_sdk.a /usr/local/lib/libminigit_sdk.a

# Setup working directory and minigit workspace
WORKDIR /data
VOLUME ["/data"]

ENTRYPOINT ["minigit"]
CMD ["--help"]
