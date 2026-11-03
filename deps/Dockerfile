# syntax=docker/dockerfile:1.7

ARG BASE_IMAGE=cgr.dev/chainguard/wolfi-base:latest
ARG BUILD_JOBS=8
ARG DROGON_VERSION=1.9.14

FROM ${BASE_IMAGE} AS toolchain

ARG BUILD_JOBS

ENV PATH=/usr/lib/ninja-build/bin:${PATH} \
    CMAKE_BUILD_PARALLEL_LEVEL=${BUILD_JOBS} \
    CMAKE_PREFIX_PATH=/usr/local \
    LD_LIBRARY_PATH=/usr/local/lib

RUN apk add --no-cache \
      build-base \
      c-ares-dev \
      cmake \
      git \
      jsoncpp-dev \
      ninja-build \
      openssl-dev \
      pkgconf \
      sqlite-dev \
      tini \
      util-linux-dev \
      zlib-dev \
    && cmake --version \
    && ninja --version \
    && g++ --version

WORKDIR /tmp/deps

FROM toolchain AS drogon

ARG BUILD_JOBS
ARG DROGON_VERSION

RUN git clone --depth 1 --branch "v${DROGON_VERSION}" --recurse-submodules --shallow-submodules https://github.com/drogonframework/drogon.git \
    && cmake -S drogon -B drogon/build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_INSTALL_LIBDIR=lib \
      -DBUILD_SHARED_LIBS=ON \
      -DBUILD_CTL=ON \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_TESTING=OFF \
      -DBUILD_DOC=OFF \
      -DBUILD_ORM=OFF \
      -DBUILD_BROTLI=OFF \
      -DBUILD_YAML_CONFIG=OFF \
      -DUSE_SUBMODULE=ON \
    && cmake --build drogon/build --parallel "${BUILD_JOBS}" \
    && DESTDIR=/out cmake --install drogon/build

FROM toolchain AS deps

COPY --from=drogon /out/usr/local/ /usr/local/

RUN drogon_ctl version
