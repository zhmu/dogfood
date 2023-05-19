FROM dogfood-toolchain:latest
LABEL maintainer "Rink Springer <rink@rink.nu>"

# python for libcxx
# auto* for dash/coreutils
# coreutils: gettext, autopoint, texinfo
# e2fsprogs for the filesystem image
# dosfstools, mtools to create/fill EFI partition
RUN apt-get update && apt-get upgrade -y && \
    apt-get install --no-install-recommends -y ninja-build \
    gettext texinfo autoconf automake make \
    gcc build-essential bison flex wget ca-certificates \
    e2fsprogs dosfstools mtools pkg-config && \
    rm -rf /var/lib/apt/lists/*

# Compile CMake ourselves; the version provided in stretch is too old for us (we need
# at least 3.9)
RUN     mkdir /tmp/cmake-build && cd /tmp/cmake-build && \
        wget "https://cmake.org/files/v3.12/cmake-3.12.2.tar.gz" && \
        tar xzf cmake-3.12.2.tar.gz && \
        cd cmake-3.12.2 && ./bootstrap -- -DCMAKE_BUILD_TYPE:STRING=Release && \
        make -j16 && make install && \
        cd / && rm -rf /tmp/cmake-build

# Create an unprivileged build user; we expect that the uid/gid matches the
# work directory on the host system (inspired by
# https://gist.github.com/alkrauss48/2dd9f9d84ed6ebff9240ccfa49a80662)
RUN groupadd -g 1000 -r build && \
    useradd -r -u 1000 -g build -d /home/build -s /sbin/nologin -c "Build user" build
ENV home=/home/build
USER build
