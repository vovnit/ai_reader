# A Linux box with everything the Kindle cross-build needs: the pre-built
# koxtoolchain is downloaded into it, and the Kindle SDK is generated on top
# (which loop-mounts a firmware image, so the container runs privileged).
FROM --platform=linux/amd64 debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates curl git sudo zstd xz-utils file \
    python3 meson ninja-build pkg-config \
    libarchive-dev nettle-dev zlib1g-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*
ENV HOME=/root
WORKDIR /work
