# Builds and runs the Qt app on Linux. Screens are drawn offscreen, so the
# smoke script's snapshots work without a display.
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config \
    qt6-base-dev libglib2.0-dev libpango1.0-dev libgdk-pixbuf-2.0-dev \
    libsqlite3-dev zlib1g-dev libcurl4-openssl-dev ca-certificates \
    fonts-dejavu fonts-liberation \
    && rm -rf /var/lib/apt/lists/*
ENV QT_QPA_PLATFORM=offscreen
WORKDIR /work
