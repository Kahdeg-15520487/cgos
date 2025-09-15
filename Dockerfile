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

# Copy setup script and run it during image build
COPY setup.sh /tmp/setup.sh
RUN dos2unix /tmp/setup.sh && \
    chmod +x /tmp/setup.sh && \
    cd /workspace && \
    /tmp/setup.sh && \
    rm /tmp/setup.sh

# Create a build script to run at container start time
COPY <<EOF /usr/local/bin/build.sh
#!/bin/bash
set -x

# Copy all files from the mounted volume to the workspace
cp -r /src/* /workspace/
cd /workspace

# Fix line endings and make scripts executable
dos2unix makeiso.sh 2>/dev/null || true
chmod +x makeiso.sh

# Run the build script directly (setup.sh already ran during image build)
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