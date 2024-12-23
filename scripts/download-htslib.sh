#!/bin/bash

HTSLIB_VERSION="1.21"
HTSLIB_SHA256="84b510e735f4963641f26fd88c8abdee81ff4cb62168310ae716636aac0f1823"

# Create directory
mkdir -p third_party/htslib

# Download if not exists
if [ ! -f "htslib-${HTSLIB_VERSION}.tar.bz2" ]; then
    curl -L -o "htslib-${HTSLIB_VERSION}.tar.bz2" "https://github.com/samtools/htslib/releases/download/${HTSLIB_VERSION}/htslib-${HTSLIB_VERSION}.tar.bz2"
fi

# Verify checksum
if command -v sha256sum > /dev/null; then
    echo "${HTSLIB_SHA256}  htslib-${HTSLIB_VERSION}.tar.bz2" | sha256sum -c
elif command -v shasum > /dev/null; then
    echo "${HTSLIB_SHA256}  htslib-${HTSLIB_VERSION}.tar.bz2" | shasum -a 256 -c
fi

# Extract only if directory doesn't exist
if [ ! -d "third_party/htslib/htslib-${HTSLIB_VERSION}" ]; then
    # Extract and configure
    cd third_party/htslib
    tar xf "../../htslib-${HTSLIB_VERSION}.tar.bz2"
    cd "htslib-${HTSLIB_VERSION}"
    autoreconf -i
fi
