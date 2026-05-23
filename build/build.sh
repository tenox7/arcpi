#!/bin/sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${IMAGE:-arc-rpi-build:latest}"
exec docker run --rm -v "$ROOT":/work -w /work/build "$IMAGE" make "$@"
