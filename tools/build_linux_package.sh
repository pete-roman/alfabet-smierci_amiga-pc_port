#!/bin/sh
# Build the Ubuntu 22.04-compatible Linux package in Docker.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
IMAGE_TAG=alfabet-smierci-linux-builder:jammy
TASK_UID=$(id -u)
TASK_GID=$(id -g)

docker build --platform linux/amd64 -f "$ROOT/tools/linux-package.Dockerfile" \
    -t "$IMAGE_TAG" "$ROOT/tools"
docker run --rm --platform linux/amd64 --user "$TASK_UID:$TASK_GID" \
    -e JOBS="${JOBS:-4}" -v "$ROOT:/src" -w /src "$IMAGE_TAG" \
    sh tools/build_linux_package_inner.sh
