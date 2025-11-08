# Lightweight Docker image for running MinUI tests
# Uses Debian Buster to match platform toolchains (GCC 8.3.0)
# Note: Uses native platform (AMD64 on CI, ARM64 on Apple Silicon)
FROM debian:buster-slim
ENV DEBIAN_FRONTEND=noninteractive

# Fix archived Debian repositories (Buster is archived)
RUN echo "deb http://archive.debian.org/debian buster main" > /etc/apt/sources.list && \
    echo "deb http://archive.debian.org/debian-security buster/updates main" >> /etc/apt/sources.list && \
    echo "Acquire::Check-Valid-Until false;" > /etc/apt/apt.conf.d/99no-check-valid-until

# Install build tools and QA dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    cppcheck \
    shellcheck \
    && rm -rf /var/lib/apt/lists/*

# Note: clang-format not available in Buster, skipping for now
# Can be added later if needed for formatting checks in Docker

# Set working directory
WORKDIR /minui

# Default command shows available test targets
CMD ["make", "-f", "Makefile.qa", "help"]
