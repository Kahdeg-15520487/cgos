FROM ubuntu:latest

# Set environment variables
ENV BUILD_TYPE=Release
ENV DEBIAN_FRONTEND=noninteractive

# Install necessary packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    gcc \
    g++ \
    make \
    nasm \
    coreutils \
    mtools \
    xorriso \
    sudo \
    dos2unix \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set up the working directory
WORKDIR /workspace

# Create a build script to run at container start time
COPY <<EOF /usr/local/bin/build.sh
#!/bin/bash
set -x

# Copy all files from the mounted volume to the workspace
cp -r /src/* /workspace/
cd /workspace

# Fix line endings and make scripts executable
dos2unix setup.sh 2>/dev/null || true
dos2unix makeiso.sh 2>/dev/null || true
chmod +x setup.sh makeiso.sh

# Modify setup script to skip package installation
sed -i '1,3s/^sudo apt-get/#sudo apt-get/' setup.sh

# Run the setup and build scripts
./setup.sh
./makeiso.sh

# Copy the ISO back to the mounted volume
cp cgos.iso /output/
EOF

RUN dos2unix /usr/local/bin/build.sh 2>/dev/null || true
RUN chmod +x /usr/local/bin/build.sh

# Set the entrypoint to the build script
ENTRYPOINT ["/bin/bash", "/usr/local/bin/build.sh"]

# Usage instructions in comments:
# Build: docker build -t cgos-builder .
# Run: docker run -v $(pwd):/src -v $(pwd):/output cgos-builder