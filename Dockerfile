# VAST
#
# VERSION               0.1

FROM debian:buster-slim as builder
LABEL maintainer="tobias.mayer@tenzir.com"
LABEL builder=true


ENV PREFIX /usr/local
ENV CC clang-7
ENV CXX clang++-7
ENV BUILD_TYPE Release
ENV BUILD_DIR /tmp/src

# Compiler and dependency setup
RUN apt-get -qq update && apt-get -qqy install \
    clang-7 libc++-dev libc++abi-dev cmake git-core
RUN apt-get -qq update && apt-get -qqy install \
    libpcap-dev libedit-dev libgoogle-perftools-dev openssl libssl-dev

# By placing the ADD directive at this point, we build both CAF and VAST
# every time. This ensures that the CI integration will always fetch a fresh
# CAF tree, regardless of the Docker cache. The correct way to handle this
# would be to provide a CAF docker image and use it in the FROM directive.
ADD . $BUILD_DIR/vast

# CAF
WORKDIR $BUILD_DIR
RUN git clone https://github.com/actor-framework/actor-framework.git caf
WORKDIR caf
RUN ./configure --prefix=$PREFIX --build-type=$BUILD_TYPE \
    --no-examples --no-opencl --no-unit-tests --no-python && \
    make -C build all install

# Broker
#WORKDIR $BUILD_DIR
#RUN git clone --recurse-submodules https://github.com/zeek/broker.git
#WORKDIR broker
#RUN ./configure --prefix=$PREFIX --with-caf=$PREFIX --build-type=$BUILD_TYPE \
#    --disable-python --disable-docs --disable-tests && \
#    make -C build all install

# VAST
WORKDIR $BUILD_DIR/vast
RUN ./configure \
       --prefix=$PREFIX \
       --build-type=$BUILD_TYPE \
       --log-level=INFO \
       --no-unit-tests && \
    make -C build all install

# Stage 2: copy application
FROM debian:buster-slim
LABEL maintainer="tobias.mayer@tenzir.com"
LABEL builder=false

ENV PREFIX /usr/local
ENV LD_LIBRARY_PATH $PREFIX/lib

RUN apt-get install -y libpcap openssl libc++abi
RUN useradd --system --uid 1337 --user-group --gid 500 tenzir
COPY --from=builder $PREFIX/ $PREFIX/

VOLUME ["/data"]

USER tenzir:tenzir
WORKDIR /data
ENTRYPOINT ["/usr/local/bin/vast"]
CMD ["--help"]
