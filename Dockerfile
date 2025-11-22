FROM ubuntu:22.04

# 환경 변수 설정 (tzdata 설치 시 대화형 프롬프트 방지)
ENV DEBIAN_FRONTEND=noninteractive
ENV EMSDK=/opt/emsdk

# 필수 도구 및 의존성 설치
RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    autoconf \
    automake \
    libtool \
    pkg-config \
    python3 \
    python3-pip \
    cmake \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

# Emscripten SDK 설치
WORKDIR /opt
RUN git clone https://github.com/emscripten-core/emsdk.git && \
    cd emsdk && \
    ./emsdk install latest && \
    ./emsdk activate latest

# Emscripten 환경 변수 설정
ENV PATH="${EMSDK}/upstream/emscripten:${PATH}"
ENV EMSDK_PYTHON=/usr/bin/python3

# 작업 디렉토리 설정
WORKDIR /workspace

# 빌드 스크립트 생성
RUN echo '#!/bin/bash\n\
set -e\n\
source /opt/emsdk/emsdk_env.sh\n\
echo "Generating configure script..."\n\
autoreconf -fiv\n\
echo "Configuring with Emscripten..."\n\
emconfigure ./configure --enable-emscripten --disable-doc\n\
echo "Building..."\n\
emmake make -j$(nproc) -k\n\
echo "Build complete! Output files are in src/ directory."' > /usr/local/bin/build.sh && \
    chmod +x /usr/local/bin/build.sh

# 기본 명령어
CMD ["/bin/bash"]

