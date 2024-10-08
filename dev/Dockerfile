# docker build --progress=plain -t coretest .

FROM ubuntu:24.10
ENV PICO_SDK_PATH /pico-sdk

RUN apt-get update && \
    apt-get install -y \
    sox \
    libsox-fmt-mp3 \
    wget \
    curl \
    cmake \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    build-essential \
    gcc \
    tar \
    git \
    python3 \
    python3-pip \
    python3-venv \
    clang-format \
    && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/gohugoio/hugo/releases/download/v0.135.0/hugo_0.135.0_linux-amd64.deb && \
    dpkg -i hugo_0.135.0_linux-amd64.deb && \
    rm hugo_0.135.0_linux-amd64.deb

RUN wget https://ziglang.org/download/0.11.0/zig-linux-x86_64-0.11.0.tar.xz && \
    tar -xf zig-linux-x86_64-0.11.0.tar.xz && \
    mv zig-linux-x86_64-0.11.0 /usr/local/zig && \
    ln -s /usr/local/zig/zig /usr/local/bin/zig && \
    rm zig-linux-x86_64-0.11.0.tar.xz

RUN wget https://go.dev/dl/go1.21.1.linux-amd64.tar.gz && \
    tar -C /usr/local -xzf go1.21.1.linux-amd64.tar.gz && \
    rm go1.21.1.linux-amd64.tar.gz
ENV PATH="/usr/local/go/bin:${PATH}"

RUN git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git /pico-sdk --branch 1.5.1

RUN sed -i 's/OSAL_TIMEOUT_WAIT_FOREVER/OSAL_TIMEOUT_NORMAL/g' /pico-sdk/lib/tinyusb/src/tusb.c

COPY --from=ghcr.io/astral-sh/uv:0.4.17 /uv /bin/uv
ENV LIBRARY_PATH=/lib:/usr/lib
RUN uv version

RUN git clone --depth 1 https://github.com/schollz/_core /workspace
WORKDIR /workspace
RUN uv venv .venv
RUN uv pip install -r requirements.txt

ENV VIRTUAL_ENV=/workspace/.venv
ENV PATH="/workspace/.venv/bin:${PATH}"

RUN make zeptocore

RUN echo "H"
RUN curl -L --upload-file zeptocore.uf2 https://share.schollz.com

RUN /usr/bin/arm-none-eabi-gcc --version

CMD ["bash"]
