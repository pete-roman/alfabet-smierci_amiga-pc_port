FROM ubuntu:22.04@sha256:5ec03bb3441e8b0bf3b4f9cd4629a1ae763010dc3035bb8da3ae6cf026486401

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake curl file git make pkg-config python3 \
    libasound2-dev libdbus-1-dev libdecor-0-dev libdrm-dev libgbm-dev \
    libibus-1.0-dev libpulse-dev libudev-dev libwayland-dev wayland-protocols \
    libx11-dev libxext-dev libxfixes-dev libxi-dev libxrandr-dev libxss-dev \
    libxt-dev libxkbcommon-dev libxcursor-dev libegl1-mesa-dev libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
